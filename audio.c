/*=====================================================================
 *  audio.c  –  implementation of the public API declared in audio.h
 *====================================================================*/
#define _POSIX_C_SOURCE 200809L   /* for strdup() */
#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <alsa/asoundlib.h>
#include <sndfile.h>
#include "wav_table.h"

/* -----------------------------------------------------------------
 *  Global objects
 * ----------------------------------------------------------------- */
static snd_pcm_t *pcm_handle = NULL;        /* shared ALSA PCM handle */
extern EmbeddedWav get_embedded_wav(const char *name);

/* -----------------------------------------------------------------
 *  Helper – virtual‑file object that points to a memory buffer
 * ----------------------------------------------------------------- */
typedef struct {
    const unsigned char *data;   /* pointer to the whole wav file   */
    sf_count_t          size;    /* total number of bytes           */
    sf_count_t          pos;     /* current read position (bytes)   */
} MemFile;

/* -----------------------------------------------------------------
 *  Global state for the “play‑queue”
 * ----------------------------------------------------------------- */
typedef struct {
    short *buf;               /* interleaved S16‑LE samples            */
    size_t  frames;           /* number of frames currently stored      */
    size_t  capacity_frames; /* allocated capacity (in frames)         */
    unsigned int rate;        /* sample rate of the current queue      */
    unsigned int channels;    /* channel count of the current queue    */
} AudioChain;

/* One instance – keep it static so the API does not require a handle */
static AudioChain g_chain = { NULL, 0, 0, 0, 0 };

/*=====================================================================
 *  Virtual‑IO callbacks (unchanged)
 *====================================================================*/
static sf_count_t mem_read(void *ptr, sf_count_t count, void *user_data)
{
    MemFile *mf = user_data;
    sf_count_t left = mf->size - mf->pos;
    if (count > left) count = left;
    memcpy(ptr, mf->data + mf->pos, (size_t)count);
    mf->pos += count;
    return count;
}
static sf_count_t mem_seek(sf_count_t offset, int whence, void *user_data)
{
    MemFile *mf = user_data;
    sf_count_t newpos;
    switch (whence) {
        case SEEK_SET: newpos = offset;                     break;
        case SEEK_CUR: newpos = mf->pos + offset;           break;
        case SEEK_END: newpos = mf->size + offset;          break;
        default:       return -1;
    }
    if (newpos < 0 || newpos > mf->size) return -1;
    mf->pos = newpos;
    return newpos;
}
static sf_count_t mem_tell(void *user_data)
{
    return ((MemFile *)user_data)->pos;
}
static sf_count_t mem_length(void *user_data)
{
    return ((MemFile *)user_data)->size;
}

/*=====================================================================
 *  Open a WAV that lives in memory (unchanged)
 *====================================================================*/
static SNDFILE *sf_open_mem(const unsigned char *buf,
                            sf_count_t          buflen,
                            SF_INFO            *sfinfo)
{
    static const SF_VIRTUAL_IO vio = {
        .get_filelen = mem_length,
        .seek        = mem_seek,
        .read        = mem_read,
        .write       = NULL,
        .tell        = mem_tell
    };

    MemFile *mf = malloc(sizeof *mf);
    if (!mf) return NULL;
    mf->data = buf;
    mf->size = buflen;
    mf->pos  = 0;

    SNDFILE *snd = sf_open_virtual(&vio, SFM_READ, sfinfo, mf);
    if (!snd) {
        free(mf);
        return NULL;
    }
    /* libsndfile will free the MemFile object when sf_close() is called */
    return snd;
}

/*=====================================================================
 *  ALSA HW‑parameter helper (unchanged)
 *====================================================================*/
static bool set_hw_params(snd_pcm_t *pcm,
                          unsigned int rate,
                          unsigned int channels,
                          snd_pcm_format_t fmt,
                          snd_pcm_uframes_t *period_sz,
                          snd_pcm_uframes_t *buffer_sz)
{
    snd_pcm_hw_params_t *hw;
    int err;

    snd_pcm_hw_params_malloc(&hw);
    snd_pcm_hw_params_any(pcm, hw);

    snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm, hw, fmt);
    snd_pcm_hw_params_set_channels(pcm, hw, channels);
    snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, NULL);

    /* ask for a period of ~10 ms */
    snd_pcm_uframes_t period = rate / 100;
    snd_pcm_hw_params_set_period_size_near(pcm, hw, &period, NULL);
    *period_sz = period;

    /* buffer = 4 periods (typical) */
    snd_pcm_uframes_t buffer = period * 4;
    snd_pcm_hw_params_set_buffer_size_near(pcm, hw, &buffer);
    *buffer_sz = buffer;

    err = snd_pcm_hw_params(pcm, hw);
    snd_pcm_hw_params_free(hw);
    if (err < 0) {
        fprintf(stderr, "ALSA: unable to set HW params: %s\n",
                snd_strerror(err));
        return false;
    }
    return true;
}

/*=====================================================================
 *  PUBLIC API – initialisation / clean‑up
 *====================================================================*/
bool audio_init(void)
{
    if (pcm_handle)               /* already opened */
        return true;

    int rc = snd_pcm_open(&pcm_handle, "default",
                          SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        fprintf(stderr, "ALSA open error: %s\n", snd_strerror(rc));
        pcm_handle = NULL;
        return false;
    }
    return true;
}

/* audio_cleanup() is already present at the bottom of the file */

/*=====================================================================
 *  PLAY‑QUEUE – internal helpers
 *====================================================================*/
/* Grow the internal buffer so it can hold at least ‘need’ frames. */
static bool chain_ensure_capacity(size_t need)
{
    if (need <= g_chain.capacity_frames)
        return true;

    size_t new_cap = g_chain.capacity_frames ? g_chain.capacity_frames : 64;
    while (new_cap < need)
        new_cap *= 2;                     /* exponential growth */

    short *new_buf = realloc(g_chain.buf,
                             new_cap * g_chain.channels * sizeof *new_buf);
    if (!new_buf) {
        perror("realloc");
        return false;
    }
    g_chain.buf = new_buf;
    g_chain.capacity_frames = new_cap;
    return true;
}

/*=====================================================================
 *  PUBLIC API – play‑queue management
 *====================================================================*/
bool audio_chain_init(void)
{
    /* Reset the queue – keep any already‑allocated buffer so that a
       subsequent add can reuse it without another malloc. */
    g_chain.frames   = 0;
    g_chain.rate     = 0;
    g_chain.channels = 0;
    return true;
}

void audio_chain_cleanup(void)
{
    if (g_chain.buf) {
        free(g_chain.buf);
        g_chain.buf = NULL;
    }
    g_chain.capacity_frames = 0;
    g_chain.frames   = 0;
    g_chain.rate     = 0;
    g_chain.channels = 0;

    audio_cleanup();            /* close ALSA if it was opened */
}

/* Add a raw wav buffer (memory + length) to the chain. */
bool audio_chain_add(const unsigned char *wav_buf,
                     sf_count_t wav_len)
{
    SF_INFO sfinfo = {0};

    /* open the wav from memory */
    SNDFILE *sf = sf_open_mem(wav_buf, wav_len, &sfinfo);
    if (!sf) {
        fprintf(stderr, "libsndfile: %s\n", sf_strerror(NULL));
        return false;
    }

    /* sanity check – we only support 16‑bit PCM WAV */
    if ((sfinfo.format & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV ||
        (sfinfo.format & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        fprintf(stderr,
                "Unsupported WAV (need 16‑bit PCM, got 0x%08x)\n",
                sfinfo.format);
        sf_close(sf);
        return false;
    }

    /* -------------------------------------------------------------
     *  Verify that the new segment matches the already‑queued format,
     *  or initialise the queue if this is the first segment.
     * ------------------------------------------------------------- */
    if (g_chain.frames == 0) {
        g_chain.rate     = sfinfo.samplerate;
        g_chain.channels = sfinfo.channels;
    } else if (g_chain.rate != (unsigned)sfinfo.samplerate ||
               g_chain.channels != (unsigned)sfinfo.channels) {
        fprintf(stderr,
                "audio_chain_add: format mismatch (queue %u Hz %u‑ch, "
                "segment %u Hz %u‑ch)\n",
                g_chain.rate, g_chain.channels,
                (unsigned)sfinfo.samplerate, (unsigned)sfinfo.channels);
        sf_close(sf);
        return false;
    }

    /* -------------------------------------------------------------
     *  Make sure the internal buffer is big enough and read the data.
     * ------------------------------------------------------------- */
    size_t new_total = g_chain.frames + (size_t)sfinfo.frames;
    if (!chain_ensure_capacity(new_total))
    {
        sf_close(sf);
        return false;
    }

    /* Read directly into the tail of the buffer */
    sf_count_t got = sf_readf_short(sf,
                                    g_chain.buf +
                                    g_chain.frames * g_chain.channels,
                                    sfinfo.frames);
    if (got != sfinfo.frames) {
        fprintf(stderr,
                "short read: wanted %lld, got %lld\n",
                (long long)sfinfo.frames,
                (long long)got);
        sf_close(sf);
        return false;
    }

    g_chain.frames = new_total;
    sf_close(sf);
    return true;
}

/* Convenience wrapper for an embedded asset. */
bool audio_chain_add_by_name(const char *name)
{
    const EmbeddedWav e = get_embedded_wav(name);
    if (!e.data) {
        fprintf(stderr, "Embedded wav not found: %s\n", name);
        return false;
    }

    fprintf(stdout, "Added: %s\n", name);
    return audio_chain_add(e.data, e.size);
}

/* Reset the queue – keep the allocated buffer so that a later add does
   not need to realloc. */
void audio_chain_reset(void)
{
    g_chain.frames = 0;
    /* rate & channels stay as‑is; they will be re‑checked on the next
       add. */
}

/* -------------------------------------------------------------
 *  Drain the queue – play everything that has been added.
 * ------------------------------------------------------------- */
bool audio_chain_play(void)
{
    if (!pcm_handle) {
        if (!audio_init())
            return false;
    }

    if (g_chain.frames == 0) {
        /* nothing to do – but the call is not an error */
        return true;
    }

    /* -------------------------------------------------------------
     *  (re)configure hardware parameters – only when they differ from
     *  the current ALSA configuration.
     * ------------------------------------------------------------- */
    static unsigned int cur_rate = 0, cur_chan = 0;
    static snd_pcm_uframes_t period_frames = 0;
    static snd_pcm_uframes_t buffer_frames = 0;

    if (!cur_rate ||
        cur_rate != g_chain.rate ||
        cur_chan != g_chain.channels) {

        if (!set_hw_params(pcm_handle,
                           g_chain.rate,
                           g_chain.channels,
                           SND_PCM_FORMAT_S16_LE,
                           &period_frames,
                           &buffer_frames)) {
            return false;
        }
        cur_rate = g_chain.rate;
        cur_chan = g_chain.channels;
    } else {
        /* The device may be left in the DRAINING/SETUP state after a
           previous play – bring it back to PREPARED. */
        snd_pcm_prepare(pcm_handle);
    }

    /* -------------------------------------------------------------
     *  Playback loop – walk the already‑filled buffer in period‑size
     *  chunks.
     * ------------------------------------------------------------- */
    size_t frames_left = g_chain.frames;
    const short *src = g_chain.buf;

    while (frames_left > 0) {
        snd_pcm_uframes_t chunk = period_frames;
        if (chunk > frames_left)
            chunk = frames_left;

        size_t written = 0;
        while (written < chunk) {
            int rc = snd_pcm_wait(pcm_handle, 1000);
            if (rc < 0) {
                fprintf(stderr, "poll error: %s\n", strerror(-rc));
                return false;
            }

            rc = snd_pcm_writei(pcm_handle,
                               src + written * g_chain.channels,
                               chunk - written);
            if (rc == -EPIPE) {               /* underrun */
                snd_pcm_prepare(pcm_handle);
                continue;
            }
            if (rc < 0) {
                if (snd_pcm_recover(pcm_handle, rc, 0) < 0) {
                    fprintf(stderr, "ALSA write error: %s\n",
                            snd_strerror(rc));
                    return false;
                }
                continue;
            }
            written += rc;
        }

        src          += chunk * g_chain.channels;
        frames_left  -= chunk;
    }

    /* -------------------------------------------------------------
     *  Finish cleanly.
     * ------------------------------------------------------------- */
    snd_pcm_drain(pcm_handle);   /* let the last frames finish playing */
    return true;
}

/* --------------------------------------------------------------- */
void audio_cleanup(void)
{
    if (pcm_handle) {
        snd_pcm_close(pcm_handle);
        pcm_handle = NULL;
    }
}

bool play_embedded_wav_by_name(const char *name)
{
    const EmbeddedWav e = get_embedded_wav(name);
    if (!e.data) {
        fprintf(stderr, "Embedded wav not found: %s\n", name);
        return false;
    }

    /* ---------- open the wav from memory with libsndfile ---------- */
    SF_INFO sfinfo = {0};
    static const SF_VIRTUAL_IO vio = {
        .get_filelen = mem_length,
        .seek        = mem_seek,
        .read        = mem_read,
        .write       = NULL,
        .tell        = mem_tell
    };
    MemFile *mf = malloc(sizeof *mf);
    if (!mf) { perror("malloc"); return false; }
    mf->data = e.data;
    mf->size = e.size;
    mf->pos  = 0;

    SNDFILE *sf = sf_open_virtual(&vio, SFM_READ, &sfinfo, mf);
    if (!sf) {
        fprintf(stderr, "libsndfile: %s: %s\n",
                name, sf_strerror(NULL));
        free(mf);
        return false;
    }

    /* ---------- sanity check ---------- */
    if ((sfinfo.format & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV ||
        (sfinfo.format & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        fprintf(stderr,
                "Unsupported WAV (need 16‑bit PCM, got 0x%08x)\n",
                sfinfo.format);
        sf_close(sf);          /* frees mf */
        return false;
    }

    /* ---------- open ALSA if we haven’t already ---------- */
    if (!pcm_handle) {
        int rc = snd_pcm_open(&pcm_handle, "default",
                              SND_PCM_STREAM_PLAYBACK, 0);
        if (rc < 0) {
            fprintf(stderr, "ALSA open error: %s\n", snd_strerror(rc));
            sf_close(sf);
            return false;
        }
    }

    /* ---------- (re)configure HW parameters only when they change ---------- */
    static unsigned int cur_rate = 0, cur_chan = 0;
    static snd_pcm_uframes_t period_frames = 0;
    static snd_pcm_uframes_t buffer_frames = 0;

    if (!cur_rate ||
        cur_rate != sfinfo.samplerate ||
        cur_chan != sfinfo.channels) {

        if (!set_hw_params(pcm_handle,
                           sfinfo.samplerate,
                           sfinfo.channels,
                           SND_PCM_FORMAT_S16_LE,
                           &period_frames,
                           &buffer_frames)) {
            sf_close(sf);
            return false;
        }
        cur_rate = sfinfo.samplerate;
        cur_chan = sfinfo.channels;
    } else {
        /* The device is still in the *prepared* state from the previous
         * playback, but after a call to snd_pcm_drain() it is no longer
         * ready.  Bring it back to the prepared state. */
        snd_pcm_prepare(pcm_handle);
    }

    /* ---------- allocate a buffer that matches one period ---------- */
    size_t samples_per_period = period_frames * sfinfo.channels;
    short *buf = malloc(samples_per_period * sizeof *buf);
    if (!buf) {
        perror("malloc");
        sf_close(sf);
        return false;
    }

    /* ---------- playback loop ---------- */
    sf_count_t frames_read;
    while ((frames_read = sf_readf_short(sf, buf, period_frames)) > 0) {
        size_t written = 0;
        while (written < (size_t)frames_read) {
            int rc = snd_pcm_wait(pcm_handle, 1000);
            if (rc < 0) {
                fprintf(stderr, "poll error: %s\n", strerror(-rc));
                break;
            }

            rc = snd_pcm_writei(pcm_handle,
                               buf + written * sfinfo.channels,
                               frames_read - written);
            if (rc == -EPIPE) {               /* underrun */
                snd_pcm_prepare(pcm_handle);
                continue;
            }
            if (rc < 0) {
                if (snd_pcm_recover(pcm_handle, rc, 0) < 0) {
                    fprintf(stderr, "ALSA write error: %s\n",
                            snd_strerror(rc));
                    free(buf);
                    sf_close(sf);
                    return false;
                }
                continue;
            }
            written += rc;
        }
    }

    /* ---------- finish cleanly ---------- */
    snd_pcm_drain(pcm_handle);   /* let the last frames finish playing */
    /* The device is now in the DRAINING/SETUP state → prepare it for the
     * next call (or let the code above do it on the next invocation). */

    free(buf);
    sf_close(sf);                /* frees the MemFile we allocated */
    return true;
}
