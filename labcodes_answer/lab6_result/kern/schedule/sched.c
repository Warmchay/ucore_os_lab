#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <stdio.h>
#include <assert.h>
#include <default_sched.h>

// the list of timer
static list_entry_t timer_list;

static struct sched_class *sched_class;

static struct run_queue *rq;

static inline void
sched_class_enqueue(struct proc_struct *proc) {
    if (proc != idleproc) {
    	// 如果不是idleproc，令proc线程加入就绪队列
        sched_class->enqueue(rq, proc);
    }
}

static inline void
sched_class_dequeue(struct proc_struct *proc) {
	// 将proc线程从就绪队列中移除
    sched_class->dequeue(rq, proc);
}

static inline struct proc_struct *
sched_class_pick_next(void) {
	// 有调度框架从就绪队列中挑选出下一个进行调度的线程
    return sched_class->pick_next(rq);
}

void
sched_class_proc_tick(struct proc_struct *proc) {
    if (proc != idleproc) {
    	// 处理时钟中断，令调度框架更新对应的调度参数
        sched_class->proc_tick(rq, proc);
    }
    else {
    	// idleproc处理时钟中断，需要进行调度
        proc->need_resched = 1;
    }
}

static struct run_queue __rq;

/**
 * 初始化任务调度器
 * */
void
sched_init(void) {
	// 清空定时器队列
    list_init(&timer_list);

    // 令当前的调度框架为default_sched_class(stride_schedule)
    sched_class = &default_sched_class;

    rq = &__rq;
    // 设置最大的时间片
    rq->max_time_slice = MAX_TIME_SLICE;
    // 初始化全局就绪队列
    sched_class->init(rq);

    cprintf("sched class: %s\n", sched_class->name);
}

/**
 * 唤醒线程(令对应线程进入就绪态，加入就绪队列)
 * */
void
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
    	// 如果传入的线程proc之前不是就绪态的
        if (proc->state != PROC_RUNNABLE) {
        	// 将其设置为就绪态
            proc->state = PROC_RUNNABLE;
            // 等待状态置空
            proc->wait_state = 0;
            if (proc != current) {
            	// 令对应线程加入就绪队列
                sched_class_enqueue(proc);
            }
        }
        else {
            warn("wakeup runnable process.\n");
        }
    }
    local_intr_restore(intr_flag);
}

/**
 * 就绪线程进行CPU调度
 * */
void
schedule(void) {
    bool intr_flag;
    struct proc_struct *next;
    // 暂时关闭中断，避免被中断打断，引起并发问题
    local_intr_save(intr_flag);
    {
    	// 令current线程处于不需要调度的状态
        current->need_resched = 0;
        if (current->state == PROC_RUNNABLE) {
        	// 如果当前线程依然是就绪态，将其置入就绪队列(有机会再被调度算法选出来)
            sched_class_enqueue(current);
        }

        // 通过调度算法筛选出下一个需要被调度的线程
        if ((next = sched_class_pick_next()) != NULL) {
        	// 如果选出来了，将其从就绪队列中出队
            sched_class_dequeue(next);
        }
        if (next == NULL) {
        	// 没有找到任何一个就绪线程，则由idleproc获得CPU
            next = idleproc;
        }
        // 被选中进行调度执行的线程，被调度执行次数+1
        next->runs ++;
        if (next != current) {
        	// 如果被选出来的线程不是current当前正在执行的线程，进行线程上下文切换，令被选中的next线程获得CPU
            proc_run(next);
        }
    }
    local_intr_restore(intr_flag);
}

