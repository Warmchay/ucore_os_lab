#ifndef __KERN_SYNC_WAIT_H__
#define __KERN_SYNC_WAIT_H__

#include <list.h>

/**
 * 等待队列
 * */
typedef struct {
	// 等待队列的头结点(哨兵节点)
    list_entry_t wait_head;
} wait_queue_t;

struct proc_struct;

/**
 * 等待队列节点项
 * */
typedef struct {
	// 关联的线程
    struct proc_struct *proc;
    // 唤醒标识
    uint32_t wakeup_flags;
    // 该节点所属的等待队列
    wait_queue_t *wait_queue;
    // 等待队列节点
    list_entry_t wait_link;
} wait_t;

#define le2wait(le, member)         \
    to_struct((le), wait_t, member)

void wait_init(wait_t *wait, struct proc_struct *proc);
void wait_queue_init(wait_queue_t *queue);
void wait_queue_add(wait_queue_t *queue, wait_t *wait);
void wait_queue_del(wait_queue_t *queue, wait_t *wait);

wait_t *wait_queue_next(wait_queue_t *queue, wait_t *wait);
wait_t *wait_queue_prev(wait_queue_t *queue, wait_t *wait);
wait_t *wait_queue_first(wait_queue_t *queue);
wait_t *wait_queue_last(wait_queue_t *queue);

bool wait_queue_empty(wait_queue_t *queue);
bool wait_in_queue(wait_t *wait);
void wakeup_wait(wait_queue_t *queue, wait_t *wait, uint32_t wakeup_flags, bool del);
void wakeup_first(wait_queue_t *queue, uint32_t wakeup_flags, bool del);
void wakeup_queue(wait_queue_t *queue, uint32_t wakeup_flags, bool del);

void wait_current_set(wait_queue_t *queue, wait_t *wait, uint32_t wait_state);

#define wait_current_del(queue, wait)                                       \
    do {                                                                    \
        if (wait_in_queue(wait)) {                                          \
            wait_queue_del(queue, wait);                                    \
        }                                                                   \
    } while (0)

#endif /* !__KERN_SYNC_WAIT_H__ */

