#ifndef AUDIO_H
#define AUDIO_H

/* -------------------------------------------------------------
 *  Required feature test macro for strdup() (and other POSIX
 *  symbols) – the implementation file defines it before any
 *  includes, but we repeat it here for users that include this
 *  header directly.
 * ------------------------------------------------------------- */
#define _POSIX_C_SOURCE 200809L

/* -------------------------------------------------------------
 *  System / library headers needed for the public API
 * ------------------------------------------------------------- */
#include <stdbool.h>          /* bool, true, false               */
#include <stddef.h>           /* size_t, NULL                    */
#include <sndfile.h>          /* sf_count_t, SF_INFO, …          */
#include <alsa/asoundlib.h>   /* snd_pcm_t, snd_pcm_format_t …   */
#include "wav_table.h"        /* EmbeddedWAV   */

/* the table and its length are provided elsewhere */
extern const EmbeddedWav embedded_wavs[];
extern const size_t      embedded_wavs_counts;

/* -----------------------------------------------------------------
 *  Lookup routine for the generated table.
 *  Implemented in the .c that contains the embedded data.
 * ----------------------------------------------------------------- */
extern EmbeddedWav get_embedded_wav(const char *name);

/* -----------------------------------------------------------------
 *  Public API – single‑instance, “handle‑less” design.
 * ----------------------------------------------------------------- */

/* Initialise the ALSA device (opens the default PCM). */
bool audio_init(void);

/* Close the ALSA device and release any internal resources. */
void audio_cleanup(void);

/* -----------------------------------------------------------------
 *  “Play‑queue” – build a playlist of WAV segments that share the
 *  same sample‑rate and channel count, then play them back as one
 *  stream.
 * ----------------------------------------------------------------- */
bool audio_chain_init(void);                     /* reset internal state   */
void audio_chain_cleanup(void);                  /* free allocated buffer  */

bool audio_chain_add(const unsigned char *wav_buf,
                     sf_count_t wav_len);       /* add raw memory block   */

bool audio_chain_add_by_name(const char *name);  /* add embedded asset     */

bool audio_chain_play(void);                     /* drain the queue        */

void audio_chain_reset(void);                    /* drop queued frames     */

/* -----------------------------------------------------------------
 *  One‑shot playback helpers (no queue, just play the given buffer).
 * ----------------------------------------------------------------- */
bool audio_play_wav_mem(const unsigned char *wav_buf,
                        sf_count_t wav_len);

/* Play an embedded WAV identified by its name (no queue). */
bool play_embedded_wav_by_name(const char *name);

#endif /* AUDIO_H */
