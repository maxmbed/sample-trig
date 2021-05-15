#include "sample_trig.h"
#include "log.h"

#define SAMPLE_TRIG_MQEUE_NAME "/trigger"
#define SAMPLE_TRIG_PCM_NAME   "default"

typedef enum bounce_dir {
    BOUNCE_UP =   -1,
    BOUNCE_DOWN = +1,
} bounce_dir_e;

typedef struct bounce_step {
    float value;
    float offset_mult;
} bounce_step_t;

typedef struct bounce_pos {
    const float origin;
    float       current;
    float       offset;
    float       limit_min;
    float       limit_max;
}bounce_pos_t;

typedef struct bounce {
    bounce_pos_t    position;
    bounce_step_t   step;
    bounce_dir_e    direction;
} bounce_t;


const char* sample_cmd_id_str[SAMPLE_ID_MAX_MSG] = {
    [SAMPLE_START]  =       "Sample start",
    [SAMPLE_DEINIT] =       "Sample deinit",
    [SAMPLE_EXITED] =       "Sample exited",
};

static long sample_trig_read_samples_cb(void* cb_data, float** samples_out);


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

    if (sample->src.handle_state != NULL) {
        LOG_INFO("Trig %d: closing sample rate converter\n", sample->id);
        src_delete(sample->src.handle_state);
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

    int ret_err = 0;
    int ret_msg = 0;
    int frame_count = 0;
    int thread_disable = 0;
    const double sample_table[4] =  {1.0, 0.5, 2.0, 1.5 };
    double sample_ratio[4] = {0};
    double bounce_step_val = 0.0;
    double bounce_origin = 1.0;
    double frame_time = 0.0;
    double sample_time = 0.0;
    double bounce_step_total = 0.0;
    int i = 0;

    memcpy(sample_ratio, sample_table, sizeof(sample_table));

    sample_trig_t* sample = (sample_trig_t*) arg;

    LOG_INFO("Starting sample Trig %d\n", sample->id);

    sample->alsa.pcm_info.channel = sample->file.info.channels;

    sample->alsa.pcm_handle = hal_alsa_pcm_open(SAMPLE_TRIG_PCM_NAME, &sample->alsa.pcm_info);
    if (sample->alsa.pcm_handle == NULL) {

        LOG_ERROR("Trig %d: Open pcm device failed\n", sample->id);
        sample_trig_clean_thread(sample);
        pthread_exit(NULL);
    }

    frame_time = (double)(sample->alsa.pcm_info.frames / (double)sample->alsa.pcm_info.rate); // the time of one pcm frame in second
    sample_time = sample->file.info.frames / (float)sample->alsa.pcm_info.rate; // just the sample time in second
    bounce_step_total = sample_time / frame_time;  // the total of bounce steps available within the sample
    bounce_step_val = bounce_origin / bounce_step_total;   //
    bounce_step_val *= 4;
    LOG_DEBUG("Trig %d: time frame         %.3fs\n", sample->id, frame_time);
    LOG_DEBUG("Trig %d: time sample        %.3fs\n", sample->id, sample_time);
    LOG_DEBUG("Trig %d: bounce step total  %.3f \n", sample->id, bounce_step_total);
    LOG_DEBUG("Trig %d: bounce step val    %.3f \n", sample->id, bounce_step_val);

    while(thread_disable == 0) {

        ret_msg = hal_mqueue_pull(&sample->mq, &sample->msg, 60);
        if (ret_msg > 0) {

            LOG_INFO("Trig %d: received audio message\n", sample->id);
            switch (sample->msg.msg_id) {

                case SAMPLE_START:

                    sample->msg.msg_id = 99;
                    sample->file.last_frame_flag = 0;

                    LOG_INFO("Trig %d: trigger sample\n", sample->id);
                    LOG_INFO("Trig %d: pcm state: %d-%s\n", sample->id, hal_get_pcm_state_int(sample->alsa.pcm_handle), hal_get_pcm_state_str(sample->alsa.pcm_handle));

                    if (hal_get_pcm_state_int(sample->alsa.pcm_handle) != 2) {
                        hal_alsa_pcm_drop_pending_samples(sample->alsa.pcm_handle);
                        snd_pcm_prepare(sample->alsa.pcm_handle);
                        LOG_INFO("Trig %d: pcm state: %d-%s\n", sample->id, hal_get_pcm_state_int(sample->alsa.pcm_handle), hal_get_pcm_state_str(sample->alsa.pcm_handle));
                    }

                    sample->src.handle_state = src_callback_new(sample_trig_read_samples_cb, SRC_LINEAR, sample->file.info.channels, &ret_err, sample);
                    if (sample->src.handle_state == NULL) {
                        LOG_ERROR("Trig %d: sample rate converter failed: %s\n", sample->id, src_strerror(ret_err));
                        continue;
                    }

                    while (!sample->file.last_frame_flag) {

                        // This were added to set the ratio per step
                        // and it must be call before src_callback_read to be effective
                        // Comment this section to use SRC auto-smooth transient
                        if (src_set_ratio(sample->src.handle_state, sample_ratio[0]) < 0) {
                            LOG_ERROR("Trig %d: sample rate set ratio failed\n", sample->id);
                        }
                        frame_count = (int)src_callback_read(sample->src.handle_state, sample_ratio[0], sample->alsa.pcm_info.frames, sample->src.buf_out);
                        if (frame_count < 0) {
                            LOG_ERROR("Trig %d: sample rate converter read failed\n", sample->id);
                        }


                        sample_ratio[0] -= bounce_step_val;
                        if (sample_ratio[0] <= 0.001) {
                            sample_ratio[0] = sample_table[0];
                        }

                        hal_alsa_pcm_wait(sample->alsa.pcm_handle);

                        hal_alsa_pcm_write(sample->alsa.pcm_handle, sample->src.buf_out, frame_count);

                        hal_mqueue_pull(&sample->mq, &sample->msg, 0);
                        if (sample->msg.msg_id == SAMPLE_START) {

                            sample->msg.msg_id = 99;
                            LOG_INFO("Trig %d: re-trigger sample\n", sample->id);
                            LOG_INFO("Trig %d: pcm state: %d-%s\n", sample->id, hal_get_pcm_state_int(sample->alsa.pcm_handle), hal_get_pcm_state_str(sample->alsa.pcm_handle));

                            hal_sndfile_reset_buff_ptr(&sample->file);
                            src_reset(sample->src.handle_state);
                            i++;
                            if (i > 3) i = 0;
                            sample_ratio[0] = sample_table[0];

                            if (hal_get_pcm_state_int(sample->alsa.pcm_handle) == 3) {
                                hal_alsa_pcm_drop_pending_samples(sample->alsa.pcm_handle);
                                snd_pcm_prepare(sample->alsa.pcm_handle);
                            }
                        } else if (sample->msg.msg_id == SAMPLE_RATIO) {
                            sample->msg.msg_id = 99;
                            //sample_ratio = sample->msg.msg_val_int;
                        } else {
                            //Nothing to handle
                        }
                    }
                    sample->file.last_frame_flag = 0;
                    i++;
                    if (i > 3) i = 0;
                    sample_ratio[0] = sample_table[0];

                    src_reset(sample->src.handle_state);
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

static long sample_trig_read_samples_cb(void* cb_data, float** samples_out) {

    sample_trig_t* sample = (sample_trig_t*)cb_data;
    int frame_count = 0;

    frame_count = hal_sndfile_read(&sample->file, sample->alsa.pcm_info.frames);
    if ((unsigned long)frame_count < sample->alsa.pcm_info.frames) {
        LOG_DEBUG("Trig %d: reach and of file\n", sample->id);
        sample->file.last_frame_flag = 1;
    }
    //memcpy(sample->src.buf_in, sample->file.buffer, frame_count);
    //*samples_out = sample->src.buf_in;
    *samples_out = sample->file.buffer;
    return frame_count;
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
