#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <linux/input.h>
#include <errno.h>

#include "log.h"
#include "hal_alsa.h"
#include "hal_mqueue.h"
#include "hal_sndfile.h"

#define SAMPLE_MESSAGE_NAME "/trigger"
#define SAMPLE_PCM_NAME     "default"

enum samples_id {
    sample_0,
    sample_1,
    sample_2,
    sample_3,
    sample_4,
    sample_5,

    samples_max,
};

enum key_code {
    key_trig_0 = 'q',
    key_trig_1 = 's',
    key_trig_2 = 'd',
    key_trig_3 = 'f',
    key_trig_4 = 'g',
    key_trig_5 = 'h',
    key_exit   = 'x',
};

typedef enum sample_msg_id {
    SAMPLE_START=0,
    SAMPLE_DEINIT,
    SAMPLE_EXITED,

    SAMPLE_ID_MAX_MSG,

} sample_msg_id_t;

const char* sample_msg_id_str[SAMPLE_ID_MAX_MSG] = {
    [SAMPLE_START]  =       "Sample start",
    [SAMPLE_DEINIT] =       "Sample deinit",
    [SAMPLE_EXITED] =       "Sample exited",
};

typedef int sample_id_t;

typedef struct sample {
    pthread_t       tid;
    sample_id_t     id;
    mq_t            mq;
    msg_t           msg;
    audio_file_t    file;
    alsa_pcm_t      alsa;

} sample_t;


const char buffer_null[1024] = {0};

void sample_file_notifier(audio_file_event_t event) {

    switch (event) {

    case last_frame_event:
        LOG_INFO("Last frame reached\n");
        break;

    default:
        LOG_INFO("audio file event uknown\n");
        break;
    }
}

void sample_free_resources(sample_t** sample_ptr, int num_resource) {

    int i = 0;
    for(i=0;i<=num_resource;i++) {

        free(sample_ptr[i]);
    }
}

void sample_clean_thread(sample_t *sample) {

    LOG_INFO("Trig %d: clean thread\n", sample->id);

    if (sample->file.handler != NULL) {
        LOG_INFO("Trig %d: closing audio files\n", sample->id);
        hal_sndfile_close(&sample->file);
    }

    if (sample->alsa.pcm_handle != NULL) {
        LOG_INFO("Trig %d: closing pcm handle\n", sample->id);
        hal_alsa_pcm_close(sample->alsa.pcm_handle);
    }

    LOG_INFO("Trig %d: de-init mqueue\n", sample->id);
    if (hal_mqueue_deinit(&sample->mq) < 0) {
        LOG_ERROR("Sample message deinit failure\n");
    }

    if (sample != NULL) {
        LOG_INFO("Trig %d: freeing sample\n", sample->id);
        free(sample);
    }
}

static void* sample_thread(void* arg) {

    if (arg == NULL) {
        LOG_ERROR("Thread argument failure\n");
        pthread_exit(NULL);
    }

    int ret_msg = 0;
    int frame_count = 0;
    int thread_disable = 0;
    int last_frame_flag = 0;

    sample_t* sample = (sample_t*) arg;

    LOG_INFO("Starting sample Trig %d\n", sample->id);

    sample->alsa.pcm_info.channel = sample->file.info.channels;

    sample->alsa.pcm_handle = hal_alsa_pcm_open(SAMPLE_PCM_NAME, &sample->alsa.pcm_info);
    if (sample->alsa.pcm_handle == NULL) {

        LOG_ERROR("Trig %d: Open pcm device failed\n", sample->id);
        sample_clean_thread(sample);
        pthread_exit(NULL);
    }


    while(thread_disable == 0) {

        ret_msg = hal_mqueue_pull(&sample->mq, &sample->msg, 60);
        if (ret_msg > 0) {

            LOG_INFO("Trig %d: received audio message\n", sample->id);
            switch (sample->msg.msg_id) {

                case SAMPLE_START:

                    sample->msg.msg_id = 99;

                    LOG_INFO("Trig %d: trigger sample\n", sample->id);
                    LOG_INFO("Trig %d: pcm state: %d-%s\n", sample->id, hal_get_pcm_state_int(sample->alsa.pcm_handle), hal_get_pcm_state_str(sample->alsa.pcm_handle));

                    if (hal_get_pcm_state_int(sample->alsa.pcm_handle) != 2) {
                        hal_alsa_pcm_drop_pending_samples(sample->alsa.pcm_handle);
                        snd_pcm_prepare(sample->alsa.pcm_handle);
                        LOG_INFO("Trig %d: pcm state: %d-%s\n", sample->id, hal_get_pcm_state_int(sample->alsa.pcm_handle), hal_get_pcm_state_str(sample->alsa.pcm_handle));
                    }

                    while (!last_frame_flag) {

                        frame_count = hal_sndfile_read(&sample->file, sample->alsa.pcm_info.frames);
                        if ((unsigned long)frame_count < sample->alsa.pcm_info.frames) {

                            last_frame_flag = 1;
                        }
                        hal_alsa_pcm_wait(sample->alsa.pcm_handle);

                        //LOG_DEBUG("Trig %d: %d/%d frame available\n", sample->id, hal_alsa_get_pcm_frame_avail(sample->alsa.pcm_handle), sample->alsa.pcm_info.buffer_size);
                        hal_alsa_pcm_write(sample->alsa.pcm_handle, sample->file.buffer, frame_count);

                        hal_mqueue_pull(&sample->mq, &sample->msg, 0);
                        if (sample->msg.msg_id == SAMPLE_START) {

                            sample->msg.msg_id = 99;
                            LOG_INFO("Trig %d: re-trigger sample\n", sample->id);
                            LOG_INFO("Trig %d: pcm state: %d-%s\n", sample->id, hal_get_pcm_state_int(sample->alsa.pcm_handle), hal_get_pcm_state_str(sample->alsa.pcm_handle));

                            hal_sndfile_reset_buff_ptr(&sample->file);

                            if (hal_get_pcm_state_int(sample->alsa.pcm_handle) == 3) {
                                hal_alsa_pcm_drop_pending_samples(sample->alsa.pcm_handle);
                                snd_pcm_prepare(sample->alsa.pcm_handle);
                            }
                        }
                    }
                    last_frame_flag = 0;
                    hal_sndfile_reset_buff_ptr(&sample->file);
                    break;

                case SAMPLE_DEINIT:
                    thread_disable = 1;
                    break;
            }
        }
    }

    int sample_id = sample->id;
    hal_mqueue_set_msg_id(&sample->msg, SAMPLE_EXITED);
    hal_mqueue_push(&sample->mq, &sample->msg);
    sample_clean_thread(sample);

    LOG_INFO("Trig %d: exiting sample thread\n", sample_id);
    pthread_exit(NULL);
}



int main(int argc, char* argv[]) {

    int i = 0;
    int ret = 0;
    int num_sample_trig = 0;
    sample_t* sample[samples_max] = {0};


    LOG_INFO("Start %s\n", argv[0]);

    if (argv[1] == NULL) {
        LOG_ERROR("Usage: %s <path sample 1> <path sample 2> ...\n", argv[0]);
        return -1;
    }

    if (argc > samples_max) {
        LOG_ERROR("Maximum %d sample allowed\n", samples_max);
        return -1;
    }

    num_sample_trig = argc -1;

    for (i=0;i< num_sample_trig;i++) {

        sample[i] = malloc(sizeof(sample_t));
        if (sample[i] == NULL) {

            LOG_ERROR("Sample %d allocation: %s\n", i, strerror(errno));
            return -1;
        }

        if (hal_sndfile_open(&sample[i]->file, argv[i+1])) {

            LOG_ERROR("Sample open failed\n");
            sample_free_resources(sample, i);
            return -1;
        }

        if (hal_sndfile_check_wav_s16_format(&sample[i]->file)) {
            LOG_ERROR("Sample format not supported for %s", argv[i+1]);
            return -1;
        }

        char sample_mq_name[CH_NAME_MAX] = {0};
        sprintf(sample_mq_name, "%s_%d", SAMPLE_MESSAGE_NAME, i);
        if (hal_mqueue_init(&sample[i]->mq, sample_mq_name, NULL) < 0) {
            LOG_ERROR("Sample message init failure\n");
            return -1;
        }

        sample[i]->id = i;
        sample[i]->msg.msg_id_str = sample_msg_id_str;
        sample[i]->msg.msg_id_max = SAMPLE_ID_MAX_MSG;

        ret = pthread_create(&sample[i]->tid, NULL, sample_thread, (void*)sample[i]);
        if (ret) {
            LOG_ERROR("Thread create: %s\n", strerror(errno));
            sample_free_resources(sample, i);
            return -1;
        }
        ret = pthread_detach(sample[i]->tid); //thread will never be joined
        if (ret) {
            LOG_ERROR("Thread detach: %s\n", strerror(errno));
            sample_free_resources(sample, i);
            return -1;
        }
    }

    sleep(1);

    hal_sndfile_set_notification_callback(sample_file_notifier);

    int quit = 0;
    char key_trig[2] = {0};

    while (quit == 0) {

        read(0, key_trig, 2);

        if (key_trig[0] == '\n')
            continue; //filter line feed

        LOG_DEBUG("Key trig: %c\n", key_trig[0]);
        switch (key_trig[0]) {

            case key_trig_0:

                if (sample[sample_0] == NULL)
                    continue;

                hal_mqueue_set_msg_id(&sample[sample_0]->msg, SAMPLE_START);

                if (hal_mqueue_push(&sample[sample_0]->mq, &sample[sample_0]->msg) < 0) {
                    LOG_ERROR("Sample message push failed\n");
                }
                break;

            case key_trig_1:

                if (sample[sample_1] == NULL)
                    continue;

                hal_mqueue_set_msg_id(&sample[sample_1]->msg, SAMPLE_START);
                if (hal_mqueue_push(&sample[sample_1]->mq, &sample[sample_1]->msg) < 0) {
                    LOG_ERROR("Sample message push failed\n");
                }
                break;

            case key_exit:

                for (i=0;i<num_sample_trig;i++) {

                    hal_mqueue_set_msg_id(&sample[i]->msg, SAMPLE_DEINIT);
                    if (hal_mqueue_push(&sample[i]->mq, &sample[i]->msg) < 0) {
                        LOG_ERROR("Sample message push failed\n");
                    }
                }

                for (i=0;i<num_sample_trig;i++) {

                    if (hal_mqueue_pull(&sample[i]->mq, &sample[i]->msg, 1) < 0) {
                        LOG_ERROR("Sample message pull failed\n");
                    }
                }

                quit = 1;
                break;

            default:
                //Otherwise, ignore other input keys
                break;
        }

        usleep(10000);
    }

    sleep(1); // TODO wait thread to exit (workaround)

    LOG_INFO("EOP\n");
    return 0;
}
