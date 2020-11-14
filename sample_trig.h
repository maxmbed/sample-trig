#include <pthread.h>
#include <fcntl.h>
#include "hal_alsa.h"
#include "hal_mqueue.h"
#include "hal_sndfile.h"

typedef enum sample_cmd_id {
    SAMPLE_START=0,
    SAMPLE_DEINIT,
    SAMPLE_EXITED,

    SAMPLE_ID_MAX_MSG,

} sample_cmd_id_t;

typedef enum samples_trig_id {
    sample_0,
    sample_1,
    sample_2,
    sample_3,
    sample_4,
    sample_5,

    samples_max,

} sample_id_t;

typedef struct sample_trig {
    pthread_t       tid;
    sample_id_t     id;
    mq_t            mq;
    msg_t           msg;
    audio_file_t    file;
    alsa_pcm_t      alsa;

} sample_trig_t;

int sample_trig_init(sample_trig_t**sample, char** list_sample, int num_sample);
int sample_trig(sample_trig_t** sample_list, sample_id_t id);
int sample_trig_exit(sample_trig_t** sample_list, int num_sample);
