#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "log.h"
#include "hal_sndfile.h"

typedef void (*sndfile_noficiation_callback_t)(audio_file_event_t event);

sndfile_noficiation_callback_t sndfile_notification_cb = NULL;


void hal_sndfile_print_info(audio_file_t* audio_file) {

    LOG_INFO("Audio file info:\n"
            " path              : %s\n"
            " numbers of frames : %ld\n"
            " sample rates      : %d\n"
            " channels          : %d\n"
            " format            : %d\n"
            " sections          : %d\n"
            " seekable          : %d\n"
            "\n",
            audio_file->path,
            audio_file->info.frames,
            audio_file->info.samplerate,
            audio_file->info.channels,
            audio_file->info.format,
            audio_file->info.sections,
            audio_file->info.seekable);

}

void hal_sndfile_set_notification_callback(void* cb_fct) {

    sndfile_notification_cb = (sndfile_noficiation_callback_t)cb_fct;
}

int hal_sndfile_open(audio_file_t* audio_file, char* file_path) {

    audio_file->handler = sf_open(file_path, SFM_READ, &audio_file->info);
    if (audio_file->handler == NULL) {
        perror("Open audio file");
        return -1;
    }

    audio_file->buffer = malloc(audio_file->info.frames * audio_file->info.channels);
    if (audio_file->buffer == NULL) {
        LOG_ERROR("Allocate audio buffer: %s\n", strerror(errno));
        return -1;
    }

    audio_file->path = malloc(strlen(file_path)+1);
    strncpy(audio_file->path, file_path, strlen(file_path)+1);

    LOG_INFO("Audio file open succeeded\n");
    hal_sndfile_print_info(audio_file);

    return 0;
}

int hal_sndfile_close(audio_file_t* audio_file) {

    int ret = 0;

    if (sf_close(audio_file->handler)) {

        LOG_ERROR("Audio file close failure\n");
        ret = -1;
    }

    free(audio_file->buffer);

    return ret;
}

long int hal_sndfile_read(audio_file_t* audio_file, sf_count_t num_frames) {

    sf_count_t frame_count;

    frame_count = sf_readf_short(audio_file->handler, audio_file->buffer, num_frames);
    if (frame_count < num_frames) {

        if (sndfile_notification_cb != NULL) {

            sndfile_notification_cb(last_frame_event);
        }
    }
    return frame_count;
}

int hal_sndfile_reset_buff_ptr(audio_file_t* audio_file) {

    if (sf_seek(audio_file->handler, 0, SF_SEEK_SET) < 0) {

        LOG_ERROR("Audio file seek failure\n");
        return -1;
    }


    return 0;
}

int hal_sndfile_check_wav_s16_format(audio_file_t* audio_file) {

    if (audio_file->info.format != (SF_FORMAT_WAV | SF_FORMAT_PCM_16)) {

        LOG_ERROR("Audio file not WAV S16_LE format\n");
        return -1;
    }

    return 0;
}
