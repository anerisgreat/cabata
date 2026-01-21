#define _POSIX_C_SOURCE 200809L   /* for strdup() */
#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <sndfile.h>
#include "wav_table.h"

static snd_pcm_t *pcm_handle = NULL;   /* shared ALSA PCM handle */
extern EmbeddedWav get_embedded_wav(const char* name); //We get definition later

/* -------------------------------------------------------------
 *  Helper: virtual‑file object that points to a memory buffer
 * ------------------------------------------------------------- */
typedef struct {
    const unsigned char *data;   /* pointer to the whole wav file */
    sf_count_t          size;    /* total number of bytes  */
    sf_count_t          pos;     /* current read position   */
} MemFile;

/* read callback */
static sf_count_t mem_read(void *ptr, sf_count_t count, void *user_data)
{
    MemFile *mf = (MemFile *)user_data;
    sf_count_t left = mf->size - mf->pos;
    if (count > left) count = left;
    memcpy(ptr, mf->data + mf->pos, (size_t)count);
    mf->pos += count;
    return count;
}

/* seek callback */
static sf_count_t mem_seek(sf_count_t offset, int whence, void *user_data)
{
    MemFile *mf = (MemFile *)user_data;
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

/* tell callback */
static sf_count_t mem_tell(void *user_data)
{
    return ((MemFile *)user_data)->pos;
}

/* get length callback (optional, but nice to have) */
static sf_count_t mem_length(void *user_data)
{
    return ((MemFile *)user_data)->size;
}

/* -------------------------------------------------------------
 *  Open a WAV that lives in memory
 * ------------------------------------------------------------- */
static SNDFILE *sf_open_mem(const unsigned char *buf,
                            sf_count_t          buflen,
                            SF_INFO            *sfinfo)
{
    static const SF_VIRTUAL_IO vio = {
        .get_filelen = mem_length,
        .seek        = mem_seek,
        .read        = mem_read,
        .write       = NULL,          /* we never write */
        .tell        = mem_tell
    };

    MemFile *mf = malloc(sizeof *mf);
    if (!mf) return NULL;
    mf->data = buf;
    mf->size = buflen;
    mf->pos  = 0;

    SNDFILE *snd = sf_open_virtual(&vio, SFM_READ, sfinfo, mf);
    if (!snd) {
        free(mf);                     /* sf_open_virtual failed */
        return NULL;
    }
    /* libsndfile will free the MemFile object when sf_close() is called,
       because we stored the pointer in the "user_data" field. */
    return snd;
}

/* --------------------------------------------------------------- */
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

    /* Ask for a period of ~10 ms (≈rate/100) and let ALSA round it */
    snd_pcm_uframes_t period = rate / 100;
    snd_pcm_hw_params_set_period_size_near(pcm, hw, &period, NULL);
    *period_sz = period;                     /* remember the real period */

    /* Buffer = N * period (typical 2–4 periods) */
    snd_pcm_uframes_t buffer = period * 4;
    snd_pcm_hw_params_set_buffer_size_near(pcm, hw, &buffer);
    *buffer_sz = buffer;                     /* keep it for debugging */

    err = snd_pcm_hw_params(pcm, hw);
    snd_pcm_hw_params_free(hw);
    if (err < 0) {
        fprintf(stderr, "ALSA: unable to set HW params: %s\n",
                snd_strerror(err));
        return false;
    }
    return true;
}


/* --------------------------------------------------------------- */
bool audio_init(void)
{
    int err;
    const char *device = "default";          /* or "plug:dmix" etc. */

    err = snd_pcm_open(&pcm_handle, device,
                       SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "ALSA: cannot open PCM device %s: %s\n",
                device, snd_strerror(err));
        return false;
    }
    /* No HW parameters are set yet – they will be configured
       on the first call to audio_play_wav() (may differ per file). */
    return true;
}

/* --------------------------------------------------------------- */
bool audio_play_wav_mem(const unsigned char *wav_buf,
                        sf_count_t wav_len)
{
    SF_INFO sfinfo = {0};
    SNDFILE *sf = sf_open_mem(wav_buf, wav_len, &sfinfo);
    if (!sf) {
        fprintf(stderr, "libsndfile: %s\n",
                sf_strerror(NULL));
        return false;
    }

    /* ---------- format sanity check ---------- */
    if ((sfinfo.format & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV ||
        (sfinfo.format & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        fprintf(stderr,
                "Unsupported WAV (need 16‑bit PCM, got 0x%08x)\n",
                sfinfo.format);
        sf_close(sf);
        return false;
    }

    /* ---------- HW re‑configuration if needed ---------- */
    static unsigned int cur_rate = 0, cur_chan = 0;
    static snd_pcm_uframes_t period_frames = 0;
    static snd_pcm_uframes_t buffer_frames = 0;

    if (!cur_rate || cur_rate != sfinfo.samplerate ||
        cur_chan != sfinfo.channels) {
        if (!set_hw_params(pcm_handle,
                           sfinfo.samplerate,
                           sfinfo.channels,
                           SND_PCM_FORMAT_S16_LE,
                           &period_frames,
                           &buffer_frames)) {
            sf_close(sf);
            return false;
        } /*  */
        cur_rate   = sfinfo.samplerate;
        cur_chan   = sfinfo.channels;
    }

    /* ---------- allocate a buffer that matches the period ---------- */
    size_t samples_per_period = period_frames * sfinfo.channels;
    short *buf = malloc(samples_per_period * sizeof(short));
    if (!buf) { perror("malloc"); sf_close(sf); return false; }

    /* ---------- main playback loop ---------- */
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
            if (rc == -EPIPE) {
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
    snd_pcm_drain(pcm_handle);
    free(buf);
    sf_close(sf);
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

    /* ---------- sanity check (same as in your original code) ---------- */
    if ((sfinfo.format & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV ||
        (sfinfo.format & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        fprintf(stderr,
                "Unsupported WAV (need 16‑bit PCM, got 0x%08x)\n",
                sfinfo.format);
        sf_close(sf);
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

    /* ---------- re‑configure HW only when needed ---------- */
    static unsigned int cur_rate = 0, cur_chan = 0;
    static snd_pcm_uframes_t period_frames = 0;
    static snd_pcm_uframes_t buffer_frames = 0;

    if (!cur_rate || cur_rate != sfinfo.samplerate ||
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
    }

    /* ---------- allocate a buffer that matches one period ---------- */
    size_t samples_per_period = period_frames * sfinfo.channels;
    short *buf = malloc(samples_per_period * sizeof(short));
    if (!buf) { perror("malloc"); sf_close(sf); return false; }

    /* ---------- playback loop (identical to your original) ---------- */
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
            if (rc == -EPIPE) {                /* underrun */
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

    /* ---------- clean up ---------- */
    snd_pcm_drain(pcm_handle);
    free(buf);
    sf_close(sf);                /* this also frees the MemFile we allocated */
    return true;
}
