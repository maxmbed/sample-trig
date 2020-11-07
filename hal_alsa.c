#include <stdio.h>
#include "hal_alsa.h"
#include "log.h"

#define SAMPLE_RATE 44100

void alsa_pcm_print_info(pcm_info_t* pcm_info) {

    LOG_INFO( "pcm information\n"
            "   pcm name    : %s\n"
            "   pcm channel : %d\n"
            "   pcm rate    : %d\n"
            "   pcm frames  : %ld\n"
            "   pcm period  : %d\n"
            "   pcm state   : %s\n"
            "   pcm handler : %ld\n"
            "   pcm buffer size : %d\n",

            pcm_info->name,
            pcm_info->channel,
            pcm_info->rate,
            pcm_info->frames,
            pcm_info->period,
            pcm_info->current_state,
            (unsigned long)pcm_info->handler,
            pcm_info->buffer_size
            );
}

int alsa_pcm_get_info(snd_pcm_t* pcm_handle, pcm_info_t* pcm_info) {

    int ret = 0;
    unsigned int pcm_val = 0;
    unsigned long pcm_frames = 0;

    pcm_info->name = (char*)snd_pcm_name(pcm_handle);
    if (pcm_info->name == NULL) {

        LOG_ERROR("get pcm info name return null\n");
        return -1;
    }

    pcm_info->current_state = (char*)snd_pcm_state_name(snd_pcm_state(pcm_handle));
    if (pcm_info->current_state == NULL) {

        LOG_ERROR("get pcm info state return null\n");
        return -1;
    }

    ret = snd_pcm_hw_params_get_channels(pcm_info->handler, &pcm_val);
    if (ret) {

        LOG_ERROR("get pcm info channel: %s\n", snd_strerror(ret));
        return -1;
    }
    pcm_info->channel = pcm_val;

    ret = snd_pcm_hw_params_get_rate(pcm_info->handler, &pcm_val, NULL);
    if (ret) {

        LOG_ERROR("get pcm info rate: %s\n", snd_strerror(ret));
        return -1;
    }
    pcm_info->rate = pcm_val;

    ret = snd_pcm_hw_params_get_period_size(pcm_info->handler, &pcm_frames, NULL);
    if (ret) {

        LOG_ERROR("get pcm info period size: %s", snd_strerror(ret));
        return -1;
    }
    pcm_info->frames = pcm_frames;

    ret = snd_pcm_hw_params_get_period_time(pcm_info->handler, &pcm_val, NULL);
    if (ret) {

        LOG_ERROR("get pcm info period: %s\n", snd_strerror(ret));
        return -1;
    }
    pcm_info->period = pcm_val;

    ret = snd_pcm_hw_params_get_buffer_size(pcm_info->handler, (snd_pcm_uframes_t*)&pcm_val);
    if (ret) {

        LOG_ERROR("get pcm buffer max size: %s\n", snd_strerror(ret));
        return -1;
    }
    pcm_info->buffer_size = pcm_val;

    return 0;
}

int alsa_pcm_set_parameters(snd_pcm_t* handle, pcm_info_t* pcm_info) {

    int ret = 0;

    ret = snd_pcm_hw_params_any(handle, pcm_info->handler);
    if (ret < 0) {

        LOG_ERROR("broken configuration for playback: no configurations available: %s\n", snd_strerror(ret));
        return -1;
    }

    ret = snd_pcm_hw_params_set_access(handle, pcm_info->handler, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (ret < 0) {

        LOG_ERROR("set interleaved mode: %s\n", snd_strerror(ret));
        return -1;
    }

    ret = snd_pcm_hw_params_set_format(handle, pcm_info->handler, SND_PCM_FORMAT_S16_LE);
    if (ret < 0) {

        LOG_ERROR("set format: %s\n", snd_strerror(ret));
        return -1;
    }

    ret = snd_pcm_hw_params_set_channels(handle, pcm_info->handler, pcm_info->channel);
    if (ret < 0) {

        LOG_ERROR("set channels number: %s\n", snd_strerror(ret));
        return -1;
    }

    unsigned int sample_rate = SAMPLE_RATE;
    ret = snd_pcm_hw_params_set_rate_near(handle, pcm_info->handler, &sample_rate, 0);
    if (ret < 0) {

        LOG_ERROR("set rate: %s\n", snd_strerror(ret));
        return -1;
    }

    snd_pcm_uframes_t buffer_size = 1024 * pcm_info->channel * sizeof(short);
    ret = snd_pcm_hw_params_set_buffer_size(handle, pcm_info->handler, buffer_size);
    if (ret < 0) {

        LOG_ERROR("set buffer size: %s\n", snd_strerror(ret));
        return -1;
    }

    ret = snd_pcm_hw_params(handle, pcm_info->handler);
    if (ret < 0) {

        LOG_ERROR("set hardware parameters. %s\n", snd_strerror(ret));
        return -1;
    }

    ret = alsa_pcm_get_info(handle, pcm_info);
    if (ret) {

        LOG_ERROR("get pcm info failed\n");
        return -1;
    }

    return 0;
}

snd_pcm_t* hal_alsa_pcm_open(char* pcm_device, pcm_info_t* pcm_info) {

    int ret = 0;
    snd_pcm_t *pcm_handle = NULL;
    snd_pcm_hw_params_t *pcm_params = NULL;


    ret = snd_pcm_open(&pcm_handle, pcm_device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if(ret < 0) {

        LOG_ERROR("open \"%s\" PCM device. %s\n", pcm_device, snd_strerror(ret));
        return NULL;
    }

    snd_pcm_hw_params_alloca(&pcm_params);
    if (pcm_params == NULL) {

        LOG_ERROR("allocate pcm parameters");
        return NULL;
    }

    pcm_info->handler = pcm_params;

    ret = alsa_pcm_set_parameters(pcm_handle, pcm_info);
    if (ret < 0) {

        LOG_ERROR("alsa set parameters failed\n");
        return NULL;
    }

    LOG_INFO("pcm open succeeded\n");
    alsa_pcm_print_info(pcm_info);

    return pcm_handle;
}

snd_pcm_t* hal_alsa_pcm_fast(char* pcm_device) {

    int ret = 0;
    snd_pcm_t *pcm_handle = NULL;

    ret = snd_pcm_open(&pcm_handle, pcm_device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if(ret < 0) {

        LOG_ERROR("open \"%s\" PCM device. %s\n", pcm_device, snd_strerror(ret));
        return NULL;
    }

    return pcm_handle;
}

int hal_alsa_get_pcm_frame_avail(snd_pcm_t* pcm_handle) {

    int ret = snd_pcm_avail(pcm_handle);
    if (ret < 0) {
        LOG_ERROR("get pcm frame avail: %s\n", snd_strerror(ret));
        return -1;
    }

    return ret;
}

int hal_alsa_pcm_wait(snd_pcm_t* pcm_handle) {

    int ret = snd_pcm_wait(pcm_handle, -1);
    if (ret < 0) {
        LOG_ERROR("wait pcm device: %s\n", snd_strerror(ret));
        return -1;
    }

    return ret;
}

int hal_alsa_pcm_close(snd_pcm_t* pcm_handle) {

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);

    return 0;
}


int hal_alsa_pcm_write(snd_pcm_t* pcm_handle, const void *buffer, int frames) {

    int ret = 0;

    if (pcm_handle == NULL) {

        LOG_ERROR("pcm device handle null\n");
        return -1;
    }

    ret = snd_pcm_writei(pcm_handle, buffer, frames);
    if (ret == -EPIPE) {

        LOG_ERROR("write pcm device: over/under run\n");
        snd_pcm_prepare(pcm_handle);

    } else if (ret < 0) {

        LOG_ERROR("write pcm device: %s\n", snd_strerror(ret));
        snd_pcm_recover(pcm_handle, ret, 0);
    }

    return 0;
}

int hal_alsa_pcm_drain_pending_samples(snd_pcm_t* pcm_handle) {

    int ret = 0;

    ret = snd_pcm_drain(pcm_handle);
    if (ret < 0) {

        LOG_ERROR("drain pending samples: %s\n", snd_strerror(ret));
        return -1;
    }

    return 0;
}

int hal_alsa_pcm_drop_pending_samples(snd_pcm_t* pcm_handle) {

    int ret = 0;
    ret = snd_pcm_drop(pcm_handle);
    if (ret < 0) {
        LOG_ERROR("drop pcm pending samples: %s\n", snd_strerror(ret));
        return -1;
    }

    snd_pcm_prepare(pcm_handle);

    return 0;
}

int hal_get_pcm_state_int(snd_pcm_t* pcm_handle) {

    return (int)snd_pcm_state(pcm_handle);
}

char* hal_get_pcm_state_str(snd_pcm_t* pcm_handle) {

    return (char*)snd_pcm_state_name(snd_pcm_state(pcm_handle));
}
