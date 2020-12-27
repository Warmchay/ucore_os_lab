#include <defs.h>
#include <wait.h>
#include <atomic.h>
#include <kmalloc.h>
#include <sem.h>
#include <proc.h>
#include <sync.h>
#include <assert.h>

/**
 * 初始化信号量
 * */
void
sem_init(semaphore_t *sem, int value) {
    sem->value = value;
    // 初始化等待队列
    wait_queue_init(&(sem->wait_queue));
}

/**
 * 信号量up操作 增加信号量或唤醒被阻塞在信号量上的一个线程（如果有的话）
 * */
static __noinline void __up(semaphore_t *sem, uint32_t wait_state) {
    bool intr_flag;
    // 暂时关闭中断，保证信号量的up操作是原子操作
    local_intr_save(intr_flag);
    {
        wait_t *wait;
        if ((wait = wait_queue_first(&(sem->wait_queue))) == NULL) {
        	// 信号量的等待队列为空，说明没有线程等待在该信号量上
        	// 信号量value加1
            sem->value ++;
        }
        else {
            assert(wait->proc->wait_state == wait_state);
            // 将找到的等待线程唤醒
            wakeup_wait(&(sem->wait_queue), wait, wait_state, 1);
        }
    }
    local_intr_restore(intr_flag);
}

/**
 * 信号量down操作 扣减信号量
 * 当信号量value不足时将当前线程阻塞在信号量上，等待其它线程up操作时将其唤醒
 * */
static __noinline uint32_t __down(semaphore_t *sem, uint32_t wait_state) {
    bool intr_flag;
    // 暂时关闭中断，保证信号量的down操作是原子操作
    local_intr_save(intr_flag);
    if (sem->value > 0) {
    	// 信号量对应的value大于0，还有权使用
        sem->value --;
        local_intr_restore(intr_flag);
        return 0;
    }

    // 信号量对应的value小于等于0，需要阻塞当前线程
    wait_t __wait, *wait = &__wait;
    // 令当前线程挂在信号量的阻塞队列中
    wait_current_set(&(sem->wait_queue), wait, wait_state);
    // 恢复中断，原子操作结束
    local_intr_restore(intr_flag);

    // 当前线程进入阻塞状态了，进行一次调度
    schedule();

    local_intr_save(intr_flag);
    // 唤醒后，原子操作将当前项从信号量的等待队列中删除
    wait_current_del(&(sem->wait_queue), wait);
    local_intr_restore(intr_flag);

    if (wait->wakeup_flags != wait_state) {
    	// 如果等待线程唤醒的标识与之前设置的参数wait_state不一致，将其状态返回给调用方做进一步判断
        return wait->wakeup_flags;
    }
    return 0;
}

void
up(semaphore_t *sem) {
    __up(sem, WT_KSEM);
}

void
down(semaphore_t *sem) {
    uint32_t flags = __down(sem, WT_KSEM);
    assert(flags == 0);
}

bool
try_down(semaphore_t *sem) {
    bool intr_flag, ret = 0;
    local_intr_save(intr_flag);
    if (sem->value > 0) {
        sem->value --, ret = 1;
    }
    local_intr_restore(intr_flag);
    return ret;
}

