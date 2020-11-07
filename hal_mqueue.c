#include "hal_mqueue.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "log.h"

#define STR_MAX_BUF    16

struct timespec message_timeout; // Message timeout

mqd_t*      mq_handler_g = NULL;
int         mq_handler_num_g = 0;
const char* mq_handler_name_g = NULL;
int         mq_handler_id_max_g = 0;

// message queue attributes
const struct mq_attr ma = {
    .mq_flags   =   0,                // blocking read/write
    .mq_maxmsg  =   10,               // maximum number of messages allowed in queue
    .mq_msgsize =   sizeof(msg_t),    // messages size content
    .mq_curmsgs =   0,
};

const char** message_queue_str = NULL;

int hal_mqueue_set_id_string_ptr(const char** msg_id_string_ptr, int msg_id_max) {

    if (msg_id_string_ptr == NULL) {
        LOG_ERROR("Message id string pointer null\n");
        return -1;
    }

    message_queue_str = msg_id_string_ptr;
    mq_handler_id_max_g = msg_id_max;

    return 0;
}

const char* hal_mqueu_get_id_string(int msg_id) {

    if (message_queue_str == NULL) {
        return (const char*)"(msg list string not set)";
    }

    if (msg_id >= mq_handler_id_max_g) {
        return (const char*)"(msg id string not set)";
    }

    return message_queue_str[msg_id];
}

int hal_mqueue_init(short msg_ch_num, const char* msg_queue_name) {

    short i = 0;
    char mq_handler_name[STR_MAX_BUF] = {0};

    LOG_INFO("Initialize message queue channel\n");

    mq_handler_g = malloc(msg_ch_num);
    if (mq_handler_g == NULL) {
        LOG_ERROR("Message queue allocation failure: %s", strerror(errno));
        return -1;
    }

    for (i=0;i<msg_ch_num;i++) {

        snprintf(mq_handler_name, STR_MAX_BUF, "%s_%d", msg_queue_name, i);

        mq_handler_g[i] = mq_open(mq_handler_name, O_RDWR | O_CREAT, 0644, &ma);
        if (mq_handler_g[i] == -1) {
            LOG_ERROR("Failed to create message queue: %s\n",strerror(errno));
            return -1;
        }
    }

    mq_handler_num_g = msg_ch_num;
    LOG_INFO("%d queue channel(s) initialized\n", mq_handler_num_g);

    return 0;
}

int hal_mqueue_deinit(const char* msg_queue_name) {

    short i = 0;
    char mq_handler_name[STR_MAX_BUF] = {0};

    LOG_INFO("Deinitialise message queue\n");

    for (i=0;i<mq_handler_num_g;i++) {

        if (mq_close(mq_handler_g[i])) {
            LOG_ERROR("Failed to close message queue: %s\n",strerror(errno));
            return -1;
        }

        snprintf(mq_handler_name, STR_MAX_BUF, "%s_%d", msg_queue_name, i);

        if (mq_unlink(mq_handler_name)) {
            LOG_ERROR("Failed to unlink message queue: %s\n",strerror(errno));
            return -1;
        }
    }

    free(mq_handler_g);

    return 0;
}

int hal_mqueue_set_timestamp(msg_t* msg) {

    clock_t t = clock();
    if (t < 0) {
        LOG_ERROR("Fail to set message time stamp\n");
        return -1;
    }

    msg->msg_timestamp = t;
    return 0;
}

int hal_mqueue_push(short msg_ch, msg_t* msg) {

    if (msg == NULL) {
        LOG_ERROR("Message push handle null\n");
        return -1;
    }

    if (msg_ch > mq_handler_num_g) {
        LOG_ERROR("Message channel unknown\n");
        return -1;
    }

    hal_mqueue_set_timestamp(msg);

    LOG_INFO("Message push ch%d:id%d-%ld:'%s'\n", msg_ch, msg->msg_id, msg->msg_timestamp, hal_mqueu_get_id_string(msg->msg_id));

    if (mq_send(mq_handler_g[msg_ch], (char*)msg, sizeof(msg_t), 0) == -1) {
        LOG_ERROR("Failed to push message ch%d:id%d-%ld:'%s': %s\n", msg_ch, msg->msg_id, msg->msg_timestamp, hal_mqueu_get_id_string(msg->msg_id), strerror(errno));
        return -1;
    }

    return 0;
}

int hal_mqueue_pull(short msg_ch, msg_t* msg, int msg_timeout) {

    if (msg == NULL) {
        LOG_ERROR("message pull handle null\n");
        return -1;
    }

    if (msg_ch > mq_handler_num_g) {
        LOG_ERROR("Message channel unknown\n");
        return -1;
    }

    int msg_ret = 0;

    clock_gettime(CLOCK_REALTIME, &message_timeout);
    message_timeout.tv_sec += msg_timeout;

    msg_ret = mq_timedreceive(mq_handler_g[msg_ch],(char*)msg, sizeof(msg_t)+1, 0, &message_timeout);
    if (msg_ret < 0) {

        if (errno != ETIMEDOUT && msg_timeout != 0) {
            LOG_ERROR("Failed to wait message ch%d:id%d-%ld:'%s': %s\n", msg_ch, msg->msg_id, msg->msg_timestamp, hal_mqueu_get_id_string(msg->msg_id), strerror(errno));
            msg_ret = -1;
        }

    } else if (msg_ret >= 0) {

        LOG_INFO("Message pull ch%d:id%d-%ld:'%s'\n", msg_ch, msg->msg_id, msg->msg_timestamp, hal_mqueu_get_id_string(msg->msg_id));
    }

    if (msg_ret == -1) {
        return -1;
    }

    return msg_ret;
}
