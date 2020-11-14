#ifndef HAL_MESSAGE_QUEUE_H_
#define HAL_MESSAGE_QUEUE_H_

#include <mqueue.h>
#include <time.h>

#define CH_NAME_MAX 16

// Message queue channel structure
typedef struct mq {

    mqd_t          handle;
    char           name[CH_NAME_MAX];
    struct mq_attr attr;

} mq_t;

// Message queue message structure
typedef struct msg {

    int             msg_id;
    const char**    msg_id_str;
    int             msg_id_max;

    void*           msg_val_ptr;
    int             msg_val_int;
    clock_t         msg_timestamp;

} msg_t;


int hal_mqueue_init(mq_t* mq, const char* mq_name, struct mq_attr* attribute);
int hal_mqueue_deinit(mq_t* mq);
int hal_mqueue_push(mq_t* mq, msg_t* msg);
int hal_mqueue_pull(mq_t* mq, msg_t* msg, int msg_timeout);
void hal_mqueue_set_msg_id(msg_t* msg, int msg_id);

#endif /* HAL_MESSAGE_QUEUE_H_ */
