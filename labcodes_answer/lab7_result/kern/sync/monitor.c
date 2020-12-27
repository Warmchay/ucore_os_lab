#include <stdio.h>
#include <monitor.h>
#include <kmalloc.h>
#include <assert.h>


// Initialize monitor.
/**
 * 初始化管程
 * */
void     
monitor_init (monitor_t * mtp, size_t num_cv) {
    int i;
    assert(num_cv>0);
    mtp->next_count = 0;
    mtp->cv = NULL;
    // 管程的互斥信号量值设为1（初始化时未被锁住）
    sem_init(&(mtp->mutex), 1); //unlocked
    // 管程的协调信号量设为0，当任何一个线程发现不满足条件时，立即阻塞在该信号量上
    sem_init(&(mtp->next), 0);
    // 未条件变量分配内存空间（参数num_cv指定管程所拥有的条件变量的个数）
    mtp->cv =(condvar_t *) kmalloc(sizeof(condvar_t)*num_cv);
    assert(mtp->cv!=NULL);
    // 构造对应个数的条件变量数组
    for(i=0; i<num_cv; i++){
        mtp->cv[i].count=0;
        // 条件变量信号量初始化时设置为0，当任何一个线程发现不满足条件时，立即阻塞在该信号量上
        sem_init(&(mtp->cv[i].sem),0);
        mtp->cv[i].owner=mtp;
    }
}

// Unlock one of threads waiting on the condition variable. 
/**
 * 条件变量唤醒操作
 * 解锁（唤醒）一个等待在当前条件变量上的线程
 * */
void 
cond_signal (condvar_t *cvp) {
    //LAB7 EXERCISE1: YOUR CODE
    cprintf("cond_signal begin: cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
    /*
     *      cond_signal(cv) {
     *          if(cv.count>0) {
     *             mt.next_count ++;
     *             signal(cv.sem);
     *             wait(mt.next);
     *             mt.next_count--;
     *          }
     *       }
     */
    // 如果等待在条件变量上的线程数大于0
    if(cvp->count>0) {
        // 需要将当前线程阻塞在管程的协调信号量next上，next_count加1
        cvp->owner->next_count ++;
        // 令阻塞在条件变量上的线程进行up操作，唤醒线程
        up(&(cvp->sem));
        // 令当前线程阻塞在管程的协调信号量next上
        // 保证管程临界区中只有一个活动线程，先令自己阻塞在next信号量上；等待被唤醒的线程在离开临界区后来反过来将自己从next信号量上唤醒
        down(&(cvp->owner->next));
        // 当前线程被其它线程唤醒从down函数中返回，next_count减1
        cvp->owner->next_count --;
    }
    cprintf("cond_signal end: cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
}

// Suspend calling thread on a condition variable waiting for condition Atomically unlocks 
// mutex and suspends calling thread on conditional variable after waking up locks mutex. Notice: mp is mutex semaphore for monitor's procedures
/**
 * 条件变量阻塞等待操作
 * 令当前线程阻塞在该条件变量上，等待其它线程将其通过cond_signal将其唤醒。
 * */
void
cond_wait (condvar_t *cvp) {
    //LAB7 EXERCISE1: YOUR CODE
    cprintf("cond_wait begin:  cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
    /*
     *         cv.count ++;
     *         if(mt.next_count>0)
     *            signal(mt.next)
     *         else
     *            signal(mt.mutex);
     *         wait(cv.sem);
     *         cv.count --;
     */

    // 阻塞在当前条件变量上的线程数加1
    cvp->count++;
    if(cvp->owner->next_count > 0)
        // 对应管程中存在被阻塞的其它线程
        // 唤醒阻塞在对应管程协调信号量next中的线程
        up(&(cvp->owner->next));
    else
        // 如果对应管程中不存在被阻塞的其它线程
        // 释放对应管程的mutex二元信号量
        up(&(cvp->owner->mutex));
    // 令当前线程阻塞在条件变量上
    down(&(cvp->sem));
    // down返回，说明已经被再次唤醒，条件变量count减1
    cvp->count --;
    cprintf("cond_wait end:  cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
}
