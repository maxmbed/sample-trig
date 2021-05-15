#include "hal_mqueue.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "log.h"

// message queue attributes
const struct mq_attr mq_attr_default = {
    .mq_flags   =   0,                // blocking read/write
    .mq_maxmsg  =   10,               // maximum number of messages allowed in queue
    .mq_msgsize =   sizeof(msg_t),    // messages size content
    .mq_curmsgs =   0,
};


const char* mqueu_get_id_string(msg_t* msg, int msg_id) {

    if (msg->msg_id_str == NULL) {
        return (const char*)"(msg list string not set)";
    }

    if (msg_id >= msg->msg_id_max) {
        return (const char*)"(msg id string unknown)";
    }

    return msg->msg_id_str[msg_id];
}

struct mq_attr mqueue_attribute_get_default(void) {

    return mq_attr_default;
}

int hal_mqueue_init(mq_t* mq, const char* mq_name, struct mq_attr* attribute) {

    LOG_INFO("Initialize message queue channel %s\n", mq_name);

    if (attribute == NULL) {

        mq->attr = mqueue_attribute_get_default();
    } else {

        mq->attr = *attribute;
    }

    mq->handle = mq_open(mq_name, O_RDWR | O_CREAT, 0644, &mq_attr_default);
    if (mq->handle == -1) {
        LOG_ERROR("Failed to create message queue: %s\n",strerror(errno));
        return -1;
    }

    strncpy(mq->name, mq_name, CH_NAME_MAX-1);

    return 0;
}

int hal_mqueue_deinit(mq_t* mq) {


    LOG_INFO("Deinitialise message queue %s\n", mq->name);

    if (mq_close(mq->handle)) {
        LOG_ERROR("Failed to close message queue: %s\n",strerror(errno));
        return -1;
    }

    if (mq_unlink(mq->name)) {
        LOG_ERROR("Failed to unlink message queue: %s\n",strerror(errno));
        return -1;
    }

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

int hal_mqueue_push(mq_t* mq, msg_t* msg) {

    if (msg == NULL) {
        LOG_ERROR("Message push handle null\n");
        return -1;
    }

    hal_mqueue_set_timestamp(msg);

    LOG_INFO("Message push ch%d:id%d-%ld:'%s'\n", mq->handle, msg->msg_id, msg->msg_timestamp, mqueu_get_id_string(msg, msg->msg_id));

    if (mq_send(mq->handle, (char*)msg, sizeof(msg_t), 0) == -1) {
        LOG_ERROR("Failed to push message ch%d:id%d-%ld:'%s': %s\n", mq->handle, msg->msg_id, msg->msg_timestamp, mqueu_get_id_string(msg, msg->msg_id), strerror(errno));
        return -1;
    }

    return 0;
}

int hal_mqueue_pull(mq_t* mq, msg_t* msg, int msg_timeout) {

    if (msg == NULL) {
        LOG_ERROR("message pull handle null\n");
        return -1;
    }

    int msg_ret = 0;
    struct timespec message_timeout;

    clock_gettime(CLOCK_REALTIME, &message_timeout);
    message_timeout.tv_sec += msg_timeout;

    msg_ret = mq_timedreceive(mq->handle,(char*)msg, sizeof(msg_t)+1, 0, &message_timeout);
    if (msg_ret < 0) {

        if (errno != ETIMEDOUT && msg_timeout != 0) {
            LOG_ERROR("Failed to wait message ch%d:id%d-%ld:'%s': %s\n", mq->handle, msg->msg_id, msg->msg_timestamp, mqueu_get_id_string(msg, msg->msg_id), strerror(errno));
            msg_ret = -1;
        }

    } else if (msg_ret >= 0) {

        LOG_INFO("Message pull ch%d:id%d-%ld:'%s'\n", mq->handle, msg->msg_id, msg->msg_timestamp, mqueu_get_id_string(msg, msg->msg_id));
    }

    if (msg_ret == -1) {
        return -1;
    }

    return msg_ret;
}

void hal_mqueue_set_msg_id(msg_t* msg, int msg_id) {

    msg->msg_id = msg_id;
}
