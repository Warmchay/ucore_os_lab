#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <default_sched.h>

#define USE_SKEW_HEAP 1

/* You should define the BigStride constant here*/
/* LAB6: YOUR CODE */
#define BIG_STRIDE    0x7FFFFFFF /* ??? */

/* The compare function for two skew_heap_node_t's and the
 * corresponding procs
 * stride调度算法如果使用的是斜堆，需要进行排序以满足堆序性，所以需要实现两个线程在队列中大小比较的逻辑
 * 在stride算法中，比较的当前线程的lab6_stride
 * */
static int
proc_stride_comp_f(void *a, void *b)
{
	 // 获取lab6_run_pool节点所对应的线程控制块
     struct proc_struct *p = le2proc(a, lab6_run_pool);
     struct proc_struct *q = le2proc(b, lab6_run_pool);

     // 两个线程的lab6_stride，作为比较的依据
     int32_t c = p->lab6_stride - q->lab6_stride;
     if (c > 0) return 1;
     else if (c == 0) return 0;
     else return -1;
}

/*
 * stride_init initializes the run-queue rq with correct assignment for
 * member variables, including:
 *
 *   - run_list: should be a empty list after initialization.
 *   - lab6_run_pool: NULL
 *   - proc_num: 0
 *   - max_time_slice: no need here, the variable would be assigned by the caller.
 *
 * hint: see proj13.1/libs/list.h for routines of the list structures.
 */
static void
stride_init(struct run_queue *rq) {
    /* LAB6: YOUR CODE */

	// 初始化就绪队列
	list_init(&(rq->run_list));
    rq->lab6_run_pool = NULL;
    rq->proc_num = 0;
}

/*
 * stride_enqueue inserts the process ``proc'' into the run-queue
 * ``rq''. The procedure should verify/initialize the relevant members
 * of ``proc'', and then put the ``lab6_run_pool'' node into the
 * queue(since we use priority queue here). The procedure should also
 * update the meta date in ``rq'' structure.
 *
 * proc->time_slice denotes the time slices allocation for the
 * process, which should set to rq->max_time_slice.
 * 
 * hint: see proj13.1/libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static void
stride_enqueue(struct run_queue *rq, struct proc_struct *proc) {
	/* LAB6: YOUR CODE */
#if USE_SKEW_HEAP
	// 使用斜堆实现就绪队列(lab6中默认USE_SKEW_HEAP为真)
	// 将proc插入就绪队列，并且更新就绪队列的头元素
    rq->lab6_run_pool =
    		skew_heap_insert(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_stride_comp_f);
#else
    // 不使用斜堆实现就绪队列，而是使用双向链表实现就绪队列
    assert(list_empty(&(proc->run_link)));
    // 将proc插入就绪队列
    list_add_before(&(rq->run_list), &(proc->run_link));
#endif
     if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice) {
    	 // 入队时，如果线程之前时间片被用完进行过调度则time_slice会为0，再次入队时需要重置时间片(或者时间片未正确设置，大于了就绪队列的max_time_slice)
    	 // 令其time_slice=rq->max_time_slice(最大分配的时间片)
         proc->time_slice = rq->max_time_slice;
     }
     // 令线程和就绪队列进行关联
     proc->rq = rq;
     // 就绪队列中的就绪线程数加1
     rq->proc_num ++;
}

/*
 * stride_dequeue removes the process ``proc'' from the run-queue
 * ``rq'', the operation would be finished by the skew_heap_remove
 * operations. Remember to update the ``rq'' structure.
 *
 * hint: see proj13.1/libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static void
stride_dequeue(struct run_queue *rq, struct proc_struct *proc) {
     /* LAB6: YOUR CODE */
#if USE_SKEW_HEAP
	// 使用斜堆实现就绪队列(lab6中默认USE_SKEW_HEAP为真)
	// 将proc移除出就绪队列，并且更新就绪队列的头元素
    rq->lab6_run_pool =
          skew_heap_remove(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_stride_comp_f);
#else
    // 不使用斜堆实现就绪队列，而是使用双向链表实现就绪队列
    assert(!list_empty(&(proc->run_link)) && proc->rq == rq);
    // 将proc移除出就绪队列，并且更新就绪队列的头元素
    list_del_init(&(proc->run_link));
#endif
    // 移除完成之后，就绪队列所拥有的线程数减1
    rq->proc_num --;
}
/*
 * stride_pick_next pick the element from the ``run-queue'', with the
 * minimum value of stride, and returns the corresponding process
 * pointer. The process pointer would be calculated by macro le2proc,
 * see proj13.1/kern/process/proc.h for definition. Return NULL if
 * there is no process in the queue.
 *
 * When one proc structure is selected, remember to update the stride
 * property of the proc. (stride += BIG_STRIDE / priority)
 *
 * hint: see proj13.1/libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static struct proc_struct *
stride_pick_next(struct run_queue *rq) {
     /* LAB6: YOUR CODE */
#if USE_SKEW_HEAP
	// 使用斜堆实现就绪队列(lab6中默认USE_SKEW_HEAP为真)
    if (rq->lab6_run_pool == NULL) return NULL; // 就绪队列为空代表没找到，返回null
    // 获取就绪队列的头结点，转换为所关联的线程返回
    struct proc_struct *p = le2proc(rq->lab6_run_pool, lab6_run_pool);
#else
    // 不使用斜堆实现就绪队列，而是使用双向链表实现就绪队列
    // 获取双向链表的头结点
    list_entry_t *le = list_next(&(rq->run_list));

    if (le == &rq->run_list)
    	// 双向链表为空代表没找到，返回null
        return NULL;
     
    struct proc_struct *p = le2proc(le, run_link);
    le = list_next(le);
    // 遍历整个双向链表，找到p->lab6_stride最小的那一个（p）
    while (le != &rq->run_list)
    {
    	struct proc_struct *q = le2proc(le, run_link);
        if ((int32_t)(p->lab6_stride - q->lab6_stride) > 0){
        	// 如果线程q的lab6_stride小于当前lab6_stride最小的线程p
        	// 令p=q，即q成为当前找到的lab6_stride最小的那一个线程
            p = q;
        }
        // 指向双向链表的下一个节点，进行遍历
        le = list_next(le);
    }
#endif
    // 最终找到的线程指针p指向的是lab6_stride最小的那一个线程，即按照stride调度算法被选中的那一个线程
    if (p->lab6_priority == 0){
    	// 特权级为0比较特殊代表最低权限，一次的步进为BIG_STRIDE
    	p->lab6_stride += BIG_STRIDE;
    }else{
    	// 否则一次的步进为BIG_STRIDE / p->lab6_priority
    	// 即lab6_priority(正整数)越大，特权级越高，一次步进的就越小
    	// 更容易被stride调度算法选中，相对而言被执行的次数也就越多，因此满足了线程特权级越高，被调度越频繁的需求
    	p->lab6_stride += BIG_STRIDE / p->lab6_priority;
    }
    return p;
}

/*
 * stride_proc_tick works with the tick event of current process. You
 * should check whether the time slices for current process is
 * exhausted and update the proc struct ``proc''. proc->time_slice
 * denotes the time slices left for current
 * process. proc->need_resched is the flag variable for process
 * switching.
 */
static void
stride_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
     /* LAB6: YOUR CODE */
     if (proc->time_slice > 0) {
    	 // 如果线程所分配的时间片还没用完(time_slice大于0)，则将所拥有的的时间片减1
          proc->time_slice --;
     }
     if (proc->time_slice == 0) {
    	 // 当时间片减为0时，说明为当前线程分配的时间片已经用完，需要重新进行一次线程调度
         proc->need_resched = 1;
     }
}

struct sched_class default_sched_class = {
     .name = "stride_scheduler",
     .init = stride_init,
     .enqueue = stride_enqueue,
     .dequeue = stride_dequeue,
     .pick_next = stride_pick_next,
     .proc_tick = stride_proc_tick,
};

