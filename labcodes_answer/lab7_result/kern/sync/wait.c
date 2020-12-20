#include <defs.h>
#include <list.h>
#include <sync.h>
#include <wait.h>
#include <proc.h>

/**
 * 初始化wait_t等待队列项
 * */
void
wait_init(wait_t *wait, struct proc_struct *proc) {
	// wait项与proc建立关联
    wait->proc = proc;
    // 等待的状态
    wait->wakeup_flags = WT_INTERRUPTED;
    // 加入等待队列
    list_init(&(wait->wait_link));
}

/**
 * 初始化等待队列
 * */
void
wait_queue_init(wait_queue_t *queue) {
	// 等待队列头结点初始化
    list_init(&(queue->wait_head));
}

/**
 * 将wait节点项插入等待队列
 * */
void
wait_queue_add(wait_queue_t *queue, wait_t *wait) {
    assert(list_empty(&(wait->wait_link)) && wait->proc != NULL);
    // wait项与等待队列建立关联
    wait->wait_queue = queue;
    // 将wait项插入头结点前
    list_add_before(&(queue->wait_head), &(wait->wait_link));
}

/**
 * 将wait项从等待队列中移除
 * */
void
wait_queue_del(wait_queue_t *queue, wait_t *wait) {
    assert(!list_empty(&(wait->wait_link)) && wait->wait_queue == queue);
    list_del_init(&(wait->wait_link));
}

/**
 * 获取等待队列中wait节点的下一项
 * */
wait_t *
wait_queue_next(wait_queue_t *queue, wait_t *wait) {
    assert(!list_empty(&(wait->wait_link)) && wait->wait_queue == queue);
    list_entry_t *le = list_next(&(wait->wait_link));
    if (le != &(queue->wait_head)) {
    	// *wait的下一项不是头结点，将其返回
        return le2wait(le, wait_link);
    }
    return NULL;
}

/**
 * 获取等待队列中wait节点的前一项
 * */
wait_t *
wait_queue_prev(wait_queue_t *queue, wait_t *wait) {
    assert(!list_empty(&(wait->wait_link)) && wait->wait_queue == queue);
    list_entry_t *le = list_prev(&(wait->wait_link));
    if (le != &(queue->wait_head)) {
    	// *wait的前一项不是头结点，将其返回
        return le2wait(le, wait_link);
    }
    return NULL;
}

/**
 * 获取等待队列的第一项
 * */
wait_t *
wait_queue_first(wait_queue_t *queue) {
	// 获取头结点的下一项
    list_entry_t *le = list_next(&(queue->wait_head));
    if (le != &(queue->wait_head)) {
    	// 头结点的下一项不是头结点，将其返回
        return le2wait(le, wait_link);
    }
    // 头结点的下一项还是头结点，说明等待队列为空(只有一个wait_head哨兵节点)
    return NULL;
}

/**
 * 获取等待队列的最后一项
 * */
wait_t *
wait_queue_last(wait_queue_t *queue) {
	// 获取头结点的前一项
    list_entry_t *le = list_prev(&(queue->wait_head));
    if (le != &(queue->wait_head)) {
    	// 头结点的前一项不是头结点，将其返回
        return le2wait(le, wait_link);
    }
    // 头结点的前一项还是头结点，说明等待队列为空(只有一个wait_head哨兵节点)
    return NULL;
}

/**
 * 等待队列是否为空
 * */
bool
wait_queue_empty(wait_queue_t *queue) {
    return list_empty(&(queue->wait_head));
}

/**
 * wait项是否在等待队列中
 * */
bool
wait_in_queue(wait_t *wait) {
    return !list_empty(&(wait->wait_link));
}

/**
 * 将等待队列中的wait项对应的线程唤醒
 * */
void
wakeup_wait(wait_queue_t *queue, wait_t *wait, uint32_t wakeup_flags, bool del) {
    if (del) {
    	// 将wait项从等待队列中删除
        wait_queue_del(queue, wait);
    }
    // 设置唤醒的原因标识
    wait->wakeup_flags = wakeup_flags;
    // 唤醒对应线程
    wakeup_proc(wait->proc);
}

/**
 * 将等待队列中的第一项对应的线程唤醒
 * */
void
wakeup_first(wait_queue_t *queue, uint32_t wakeup_flags, bool del) {
    wait_t *wait;
    if ((wait = wait_queue_first(queue)) != NULL) {
        wakeup_wait(queue, wait, wakeup_flags, del);
    }
}

/**
 * 将等待队列中的所有项对应的线程全部唤醒
 * */
void
wakeup_queue(wait_queue_t *queue, uint32_t wakeup_flags, bool del) {
    wait_t *wait;
    if ((wait = wait_queue_first(queue)) != NULL) {
        if (del) {
            do {
                wakeup_wait(queue, wait, wakeup_flags, 1);
            } while ((wait = wait_queue_first(queue)) != NULL);
        }
        else {
            do {
                wakeup_wait(queue, wait, wakeup_flags, 0);
            } while ((wait = wait_queue_next(queue, wait)) != NULL);
        }
    }
}

/**
 * 令对应wait项加入当前等待队列;令当前线程阻塞休眠，挂载在该等待队列中
 * */
void
wait_current_set(wait_queue_t *queue, wait_t *wait, uint32_t wait_state) {
    assert(current != NULL);
    wait_init(wait, current);
    current->state = PROC_SLEEPING;
    current->wait_state = wait_state;
    wait_queue_add(queue, wait);
}

