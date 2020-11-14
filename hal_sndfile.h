#ifndef HAL_SNDFILE_H
#define HAL_SNDFILE_H

#include <sndfile.h>

typedef enum audio_file_event {
    last_frame_event=1,

} audio_file_event_t;

typedef struct audio_file {

    SF_INFO     info;
    SNDFILE*    handler;
    char*       path;
    short*      buffer;

} audio_file_t;

void hal_sndfile_print_info(audio_file_t* audio_file);
void hal_sndfile_set_notification_callback(void* cb_fct);

int hal_sndfile_open(audio_file_t* audio_file, char* file_path);
int hal_sndfile_close(audio_file_t* audio_file);
long int hal_sndfile_read(audio_file_t* audio_file, sf_count_t num_frames);
int hal_sndfile_reset_buff_ptr(audio_file_t* audio_file);
int hal_sndfile_check_wav_s16_format(audio_file_t* audio_file);

#endif /* HAL_SNDFILE */
