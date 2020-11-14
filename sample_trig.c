#include "sample_trig.h"
#include "log.h"

#define SAMPLE_TRIG_MQEUE_NAME "/trigger"
#define SAMPLE_TRIG_PCM_NAME   "default"

const char* sample_cmd_id_str[SAMPLE_ID_MAX_MSG] = {
    [SAMPLE_START]  =       "Sample start",
    [SAMPLE_DEINIT] =       "Sample deinit",
    [SAMPLE_EXITED] =       "Sample exited",
};


static void sample_trig_notifier(audio_file_event_t event) {

    switch (event) {

    case last_frame_event:
        LOG_INFO("Last frame reached\n");
        break;

    default:
        LOG_INFO("audio file event unknown\n");
        break;
    }
}

static void sample_trig_clean_thread(sample_trig_t *sample) {

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

static void* sample_trig_thread(void* arg) {

    if (arg == NULL) {
        LOG_ERROR("Thread argument failure\n");
        pthread_exit(NULL);
    }

    int ret_msg = 0;
    int frame_count = 0;
    int thread_disable = 0;
    int last_frame_flag = 0;

    sample_trig_t* sample = (sample_trig_t*) arg;

    LOG_INFO("Starting sample Trig %d\n", sample->id);

    sample->alsa.pcm_info.channel = sample->file.info.channels;

    sample->alsa.pcm_handle = hal_alsa_pcm_open(SAMPLE_TRIG_PCM_NAME, &sample->alsa.pcm_info);
    if (sample->alsa.pcm_handle == NULL) {

        LOG_ERROR("Trig %d: Open pcm device failed\n", sample->id);
        sample_trig_clean_thread(sample);
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
    sample_trig_clean_thread(sample);

    LOG_INFO("Trig %d: exiting sample thread\n", sample_id);
    pthread_exit(NULL);
}

static void sample_trig_free_resources(sample_trig_t** sample_ptr, int num_resource) {

    int i = 0;
    for(i=0;i<=num_resource;i++) {

        free(sample_ptr[i]);
    }
}

int sample_trig_init(sample_trig_t**sample, char** list_sample, int num_sample) {

    int i = 0;
    int ret = 0;

    for (i=0;i< num_sample;i++) {

        sample[i] = malloc(sizeof(sample_trig_t));
        if (sample[i] == NULL) {

            LOG_ERROR("Sample %d allocation: %s\n", i, strerror(errno));
            return -1;
        }

        if (hal_sndfile_open(&sample[i]->file, list_sample[i+1])) {

            LOG_ERROR("Sample open failed\n");
            sample_trig_free_resources(sample, i);
            return -1;
        }

        if (hal_sndfile_check_wav_s16_format(&sample[i]->file)) {
            LOG_ERROR("Sample format not supported for %s", list_sample[i+1]);
            return -1;
        }

        char sample_mq_name[CH_NAME_MAX] = {0};
        sprintf(sample_mq_name, "%s_%d", SAMPLE_TRIG_MQEUE_NAME, i);
        if (hal_mqueue_init(&sample[i]->mq, sample_mq_name, NULL) < 0) {
            LOG_ERROR("Sample message init failure\n");
            return -1;
        }

        sample[i]->id = i;
        sample[i]->msg.msg_id_str = sample_cmd_id_str;
        sample[i]->msg.msg_id_max = SAMPLE_ID_MAX_MSG;

        ret = pthread_create(&sample[i]->tid, NULL, sample_trig_thread, (void*)sample[i]);
        if (ret) {
            LOG_ERROR("Thread create: %s\n", strerror(errno));
            sample_trig_free_resources(sample, i);
            return -1;
        }
        ret = pthread_detach(sample[i]->tid); //thread will never be joined
        if (ret) {
            LOG_ERROR("Thread detach: %s\n", strerror(errno));
            sample_trig_free_resources(sample, i);
            return -1;
        }
    }

    hal_sndfile_set_notification_callback(sample_trig_notifier);

    return 0;
}

int sample_trig(sample_trig_t** sample_list, sample_id_t id) {

    if (sample_list[id] == NULL)
        return -1;

    hal_mqueue_set_msg_id(&sample_list[id]->msg, SAMPLE_START);

    if (hal_mqueue_push(&sample_list[id]->mq, &sample_list[id]->msg) < 0) {
        LOG_ERROR("Sample message push failed\n");
        return -1;
    }

    return 0;
}

int sample_trig_exit(sample_trig_t** sample_list, int num_sample) {

    int i = 0;

    for (i=0;i<num_sample;i++) {

        hal_mqueue_set_msg_id(&sample_list[i]->msg, SAMPLE_DEINIT);
        if (hal_mqueue_push(&sample_list[i]->mq, &sample_list[i]->msg) < 0) {
            LOG_ERROR("Sample message push failed\n");
            return -1;
        }
    }

    for (i=0;i<num_sample;i++) {

        if (hal_mqueue_pull(&sample_list[i]->mq, &sample_list[i]->msg, 1) < 0) {
            LOG_ERROR("Sample message pull failed\n");
            return -1;
        }
    }

    return 0;
}
