#ifndef AUDIO_H
#define AUDIO_H

/* ----------------------------------------------------------------------
 *  Public API – audio.c
 * ---------------------------------------------------------------------- */

#include <stdbool.h>          /* bool */
#include <stddef.h>           /* size_t */
#include <sndfile.h>          /* SF_INFO, sf_count_t, etc. */

/* -------------------------------------------------------------
 *  Embedded WAV table (defined in wav_table.c / wav_table.h)
 * ------------------------------------------------------------- */

/* -------------------------------------------------------------
 *  Public functions
 * ------------------------------------------------------------- */

/**
 * Initialise the ALSA playback device.
 *
 * Returns true on success, false on failure (error already printed).
 */
bool audio_init(void);

/**
 * Play a WAV file that lives completely in memory.
 *
 * @param wav_buf  pointer to the raw wav bytes
 * @param wav_len  length of the buffer in bytes
 *
 * Returns true on successful playback, false otherwise.
 */
bool audio_play_wav_mem(const unsigned char *wav_buf,
                        sf_count_t          wav_len);

/**
 * Play one of the pre‑embedded WAV files by its name.
 *
 * @param name  the exact string that appears in the EmbeddedWav.name field
 *
 * Returns true on success, false if the name is unknown or playback fails.
 */
bool play_embedded_wav_by_name(const char *name);

/**
 * Release the ALSA handle and any other global resources.
 * Safe to call even if audio_init() never succeeded.
 */
void audio_cleanup(void);

#endif /* AUDIO_H */
