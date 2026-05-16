#ifndef ALSA_MIXER_H
#define ALSA_MIXER_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include <alsa/asoundlib.h>
#include <stdio.h>

/*********************
 *      DEFINES
 *********************/

#define ALSA_MIXER_DEBUG(fmt, ...) printf("[ALSA MIXER] " fmt "\n", ##__VA_ARGS__)

int alsa_mixer_init(void);

#ifdef __cplusplus
}
#endif

#endif