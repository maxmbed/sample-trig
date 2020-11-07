#ifndef HAL_MESSAGE_QUEUE_H_
#define HAL_MESSAGE_QUEUE_H_

#include <mqueue.h>
#include <time.h>

// Message structure
typedef struct msg {
    int     msg_id;
    void*   msg_ptr;
    int     msg_int;
    clock_t msg_timestamp;

} msg_t;

int hal_mqueue_init(short msg_ch_num, const char* msg_queue_name);
int hal_mqueue_deinit(const char* msg_queue_name);
int hal_mqueue_set_id_string_ptr(const char** msg_id_string_ptr, int msg_id_max);
int hal_mqueue_push(short msg_ch, msg_t* msg);
int hal_mqueue_pull(short msg_ch, msg_t* msg, int msg_timeout);

#endif /* HAL_MESSAGE_QUEUE_H_ */
