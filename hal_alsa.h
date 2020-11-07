#ifndef HAL_ALSA_H
#define HAL_ALSA_H

#include <alsa/asoundlib.h>

#define PCM_MAX_NAME 255

typedef struct pcm_info {

    snd_pcm_hw_params_t*    handler;
    char*                   name;
    unsigned int            channel;
    unsigned int            rate;
    unsigned long           frames;
    unsigned int            period;
    char*                   current_state;
    unsigned int            buffer_size;

} pcm_info_t;

typedef struct alsa_pcm {

    snd_pcm_t* pcm_handle;
    pcm_info_t pcm_info;
} alsa_pcm_t;

snd_pcm_t* hal_alsa_pcm_open(char* pcm_device, pcm_info_t* pcm_info);
int hal_alsa_pcm_wait(snd_pcm_t* pcm_handle);
int hal_alsa_pcm_close(snd_pcm_t* pcm_handle);
int hal_alsa_pcm_write(snd_pcm_t* pcm_handle, const void *buffer, int frames);
int hal_alsa_pcm_drain_pending_samples(snd_pcm_t* pcm_handle);
int hal_alsa_pcm_drop_pending_samples(snd_pcm_t* pcm_handle);
int hal_get_pcm_state_int(snd_pcm_t* pcm_handle);
char* hal_get_pcm_state_str(snd_pcm_t* pcm_handle);

int hal_alsa_get_pcm_frame_avail(snd_pcm_t* pcm_handle);

#endif /* HAL_ALSA */
