#include <proc.h>
#include <kmalloc.h>
#include <string.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <vmm.h>
#include <trap.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
introduction:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one threads
for execution, the kernel data(for management), processor state (for context switch), files(in lab6), etc. ucore needs to
manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc, 
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:
                                            
  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+ 
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  + 
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : process sleep                           -->do_sleep 
SYS_kill        : kill process                            -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit   
SYS_getpid      : get the process's pid

*/

// the process set's list
list_entry_t proc_list;

#define HASH_SHIFT          10
#define HASH_LIST_SIZE      (1 << HASH_SHIFT)
#define pid_hashfn(x)       (hash32(x, HASH_SHIFT))

// has list for process set based on pid
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
struct proc_struct *idleproc = NULL;
// init proc
struct proc_struct *initproc = NULL;
// current proc
struct proc_struct *current = NULL;

static int nr_process = 0;

// kernel_thread_entry定义在/kern/process/entry.S中
void kernel_thread_entry(void);
// forkrets定义在/kern/trap/trapentry.S中的
void forkrets(struct trapframe *tf);
void switch_to(struct context *from, struct context *to);

// alloc_proc - alloc a proc_struct and init all fields of proc_struct
// 分配一个进程控制块结构
static struct proc_struct *
alloc_proc(void) {
	// 分配一个进程控制块
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    // 如果分配成功
    if (proc != NULL) {
    //LAB4:EXERCISE1 YOUR CODE
    /*
     * below fields in proc_struct need to be initialized
     *       enum proc_state state;                      // Process state
     *       int pid;                                    // Process ID
     *       int runs;                                   // the running times of Proces
     *       uintptr_t kstack;                           // Process kernel stack
     *       volatile bool need_resched;                 // bool value: need to be rescheduled to release CPU?
     *       struct proc_struct *parent;                 // the parent process
     *       struct mm_struct *mm;                       // Process's memory management field
     *       struct context context;                     // Switch here to run process
     *       struct trapframe *tf;                       // Trap frame for current interrupt
     *       uintptr_t cr3;                              // CR3 register: the base addr of Page Directroy Table(PDT)
     *       uint32_t flags;                             // Process flag
     *       char name[PROC_NAME_LEN + 1];               // Process name
     */
    	// 对分配好内存的proc结构体属性进行格式化

    	// 最一开始进程的状态为uninitialized
        proc->state = PROC_UNINIT;
        // 负数的pid是非法的，未正式初始化之前pid统一为-1
        proc->pid = -1;
        proc->runs = 0;
        proc->kstack = 0;
        proc->need_resched = 0;
        proc->parent = NULL;
        proc->mm = NULL;
        // 清零格式化proc->context中的内容
        memset(&(proc->context), 0, sizeof(struct context));
        proc->tf = NULL;
        // 未初始化时，cr3默认指向内核页表
        proc->cr3 = boot_cr3;
        proc->flags = 0;
        // 清零格式化proc->name
        memset(proc->name, 0, PROC_NAME_LEN);
    }
    return proc;
}

// set_proc_name - set the name of proc
char *
set_proc_name(struct proc_struct *proc, const char *name) {
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - get the name of proc
char *
get_proc_name(struct proc_struct *proc) {
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// get_pid - alloc a unique pid for process
// MAX_PID = 最大进程数MAX_PROCESS * 2
// 每次分配新的PID时(get_pid)，可以从一个1~MAX_PID的环形数据域内，保证获得一个当前唯一的PID)
static int
get_pid(void) {
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;

    if (++ last_pid >= MAX_PID) {
    	// 当发现上一个last_pid已经要超过MAX_PID时，从1重新开始分配（0已被idle_proc占用）
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe) {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list) {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid) {
                if (++ last_pid >= next_safe) {
                    if (last_pid >= MAX_PID) {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid) {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}

// proc_run - make process "proc" running on cpu
// NOTE: before call switch_to, should load  base addr of "proc"'s new PDT
// 进行线程调度，令当前占有CPU的让出CPU，并令参数proc指向的线程获得CPU控制权
void
proc_run(struct proc_struct *proc) {
    if (proc != current) {
    	// 只有当proc不是当前执行的线程时，才需要执行
        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;

        // 切换时新线程任务时需要暂时关闭中断，避免出现嵌套中断
        local_intr_save(intr_flag);
        {
            current = proc;
            // 设置TSS任务状态段的esp0的值，令其指向新线程的栈顶
            // ucore参考Linux的实现，不使用80386提供的TSS任务状态段这一硬件机制实现任务上下文切换，ucore在启动时初始化TSS后(init_gdt)，便不再对其进行修改。
            // 但进行中断等操作时，依然会用到当前TSS内的esp0属性。发生用户态到内核态中断切换时，硬件会将中断栈帧压入TSS.esp0指向的内核栈中
            // 因此ucore中的每个线程，需要有自己的内核栈，在进行线程调度切换时，也需要及时的修改esp0的值，使之指向新线程的内核栈顶。
            load_esp0(next->kstack + KSTACKSIZE);
            // 设置cr3寄存器的值，令其指向新线程的页表
            lcr3(next->cr3);
            // switch_to用于完整的进程上下文切换，定义在统一目录下的switch.S中
            // 由于涉及到大量的寄存器的存取操作，因此使用汇编实现
            switch_to(&(prev->context), &(next->context));
        }
        local_intr_restore(intr_flag);
    }
}

// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
static void
forkret(void) {
    forkrets(current->tf);
}

// hash_proc - add proc into proc hash_list
static void
hash_proc(struct proc_struct *proc) {
	// 令一个线程控制块加入全局的线程控制块hash表中(key = proc.pid，value = proc.hash_link)
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// find_proc - find proc frome proc hash_list according to pid
struct proc_struct *
find_proc(int pid) {
	// 根据pid从全局线程控制块hash表中查找出对应的线程控制块
    if (0 < pid && pid < MAX_PID) {
    	// 先根据pid快读定位到对应hash表中桶的位置
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        // 发生pid hash冲突时，遍历整个冲突链表，尝试找到对应的线程控制块
        while ((le = list_next(le)) != list) {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid) {
            	// 比对pid，如果匹配则找到并直接返回
                return proc;
            }
        }
    }
    // 没找到就返回NULL
    return NULL;
}

// kernel_thread - create a kernel thread using "fn" function
// NOTE: the contents of temp trapframe tf will be copied to 
//       proc->tf in do_fork-->copy_thread function
// 创建一个内核线程，并执行参数fn函数，arg作为fn的参数
int
kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags) {
    struct trapframe tf;
    // 构建一个临时的中断栈帧tf，用于do_fork中的copy_thread函数(因为线程的创建和切换是需要利用CPU中断返回机制的)
    memset(&tf, 0, sizeof(struct trapframe));
    // 设置tf的值
    tf.tf_cs = KERNEL_CS; // 内核线程，设置中断栈帧中的代码段寄存器CS指向内核代码段
    tf.tf_ds = tf.tf_es = tf.tf_ss = KERNEL_DS; // 内核线程，设置中断栈帧中的数据段寄存器指向内核数据段
    tf.tf_regs.reg_ebx = (uint32_t)fn; // 设置中断栈帧中的ebx指向fn的地址
    tf.tf_regs.reg_edx = (uint32_t)arg; // 设置中断栈帧中的edx指向arg的起始地址
    tf.tf_eip = (uint32_t)kernel_thread_entry; // 设置tf.eip指向kernel_thread_entry这一统一的初始化的内核线程入口地址
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
static int
setup_kstack(struct proc_struct *proc) {
	// 分配一个KSTACKPAGE页大小的物理空间
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL) {
    	// 作为内核栈使用
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

// put_kstack - free the memory space of process kernel stack
static void
put_kstack(struct proc_struct *proc) {
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

// copy_mm - process "proc" duplicate OR share process "current"'s mm according clone_flags
//         - if clone_flags & CLONE_VM, then "share" ; else "duplicate"
static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc) {
    assert(current->mm == NULL);
    /* do nothing in this project lab4中只有内核线程，不需要进行父子进程内存的同步 */
    return 0;
}

// copy_thread - setup the trapframe on the  process's kernel stack top and
//             - setup the kernel entry point and stack of process
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf) {
	// 令proc-tf 指向proc内核栈顶向下偏移一个struct trapframe大小的位置
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
    // 将参数tf中的结构体数据复制填入上述proc->tf指向的位置(正好是上面struct trapframe指针-1腾出来的那部分空间)
    *(proc->tf) = *tf;
    proc->tf->tf_regs.reg_eax = 0;
    proc->tf->tf_esp = esp;
    proc->tf->tf_eflags |= FL_IF;

    // 令proc上下文中的eip指向forkret,切换恢复上下文后，新线程proc便会跳转至forkret
    proc->context.eip = (uintptr_t)forkret;
    // 令proc上下文中的esp指向proc->tf，指向中断返回时的中断栈帧
    proc->context.esp = (uintptr_t)(proc->tf);
}

/* do_fork -     parent process for a new child process
 * @clone_flags: used to guide how to clone the child process
 * @stack:       the parent's user stack pointer. if stack==0, It means to fork a kernel thread.
 * @tf:          the trapframe info, which will be copied to child process's proc->tf
 */
int
do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }
    ret = -E_NO_MEM;
    //LAB4:EXERCISE2 YOUR CODE
    /*
     * Some Useful MACROs, Functions and DEFINEs, you can use them in below implementation.
     * MACROs or Functions:
     *   alloc_proc:   create a proc struct and init fields (lab4:exercise1)
     *   setup_kstack: alloc pages with size KSTACKPAGE as process kernel stack
     *   copy_mm:      process "proc" duplicate OR share process "current"'s mm according clone_flags
     *                 if clone_flags & CLONE_VM, then "share" ; else "duplicate"
     *   copy_thread:  setup the trapframe on the  process's kernel stack top and
     *                 setup the kernel entry point and stack of process
     *   hash_proc:    add proc into proc hash_list
     *   get_pid:      alloc a unique pid for process
     *   wakeup_proc:  set proc->state = PROC_RUNNABLE
     * VARIABLES:
     *   proc_list:    the process set's list
     *   nr_process:   the number of process set
     */

    //    1. call alloc_proc to allocate a proc_struct
    //    2. call setup_kstack to allocate a kernel stack for child process
    //    3. call copy_mm to dup OR share mm according clone_flag
    //    4. call copy_thread to setup tf & context in proc_struct
    //    5. insert proc_struct into hash_list && proc_list
    //    6. call wakeup_proc to make the new child process RUNNABLE
    //    7. set ret vaule using child proc's pid

    // 分配一个未初始化的线程控制块
    if ((proc = alloc_proc()) == NULL) {
        goto fork_out;
    }
    // 其父进程属于current当前进程
    proc->parent = current;

    // 设置，分配新线程的内核栈
    if (setup_kstack(proc) != 0) {
    	// 分配失败，回滚释放之前所分配的内存
        goto bad_fork_cleanup_proc;
    }
    // 由于是fork，因此fork的一瞬间父子线程的内存空间是一致的（clone_flags决定是否采用写时复制）
    if (copy_mm(clone_flags, proc) != 0) {
    	// 分配失败，回滚释放之前所分配的内存
        goto bad_fork_cleanup_kstack;
    }
    // 复制proc线程时，设置proc的上下文信息
    copy_thread(proc, stack, tf);

    bool intr_flag;
    local_intr_save(intr_flag);
    {
    	// 生成并设置新的pid
        proc->pid = get_pid();
        // 加入全局线程控制块哈希表
        hash_proc(proc);
        // 加入全局线程控制块双向链表
        list_add(&proc_list, &(proc->list_link));
        nr_process ++;
    }
    local_intr_restore(intr_flag);
    // 唤醒proc，令其处于就绪态PROC_RUNNABLE
    wakeup_proc(proc);

    ret = proc->pid;
fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}

// do_exit - called by sys_exit
//   1. call exit_mmap & put_pgdir & mm_destroy to free the almost all memory space of process
//   2. set process' state as PROC_ZOMBIE, then call wakeup_proc(parent) to ask parent reclaim itself.
//   3. call scheduler to switch to other process
int
do_exit(int error_code) {
    panic("process exit!!.\n");
}

// init_main - the second kernel thread used to create user_main kernel threads
static int
init_main(void *arg) {
    cprintf("this initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
    cprintf("To U: \"%s\".\n", (const char *)arg);
    cprintf("To U: \"en.., Bye, Bye. :)\"\n");
    return 0;
}

// proc_init - set up the first kernel thread idleproc "idle" by itself and 
//           - create the second kernel thread init_main
// 初始化第一个内核线程 idle线程、第二个内核线程 init_main线程
void
proc_init(void) {
    int i;

    // 初始化全局的线程控制块双向链表
    list_init(&proc_list);
    // 初始化全局的线程控制块hash表
    for (i = 0; i < HASH_LIST_SIZE; i ++) {
        list_init(hash_list + i);
    }

    // 分配idle线程结构
    if ((idleproc = alloc_proc()) == NULL) {
        panic("cannot alloc idleproc.\n");
    }

    // 为idle线程进行初始化
    idleproc->pid = 0; // idle线程pid作为第一个内核线程，其不会被销毁，pid为0
    idleproc->state = PROC_RUNNABLE; // idle线程被初始化时是就绪状态的
    idleproc->kstack = (uintptr_t)bootstack; // idle线程是第一个线程，其内核栈指向bootstack
    idleproc->need_resched = 1; // idle线程被初始化后，需要马上被调度
    // 设置idle线程的名称
    set_proc_name(idleproc, "idle");
    nr_process ++;

    // current当前执行线程指向idleproc
    current = idleproc;

    // 初始化第二个内核线程initproc， 用于执行init_main函数，参数为"Hello world!!"
    int pid = kernel_thread(init_main, "Hello world!!", 0);
    if (pid <= 0) {
    	// 创建init_main线程失败
        panic("create init_main failed.\n");
    }

    // 获得initproc线程控制块
    initproc = find_proc(pid);
    // 设置initproc线程的名称
    set_proc_name(initproc, "init");

    assert(idleproc != NULL && idleproc->pid == 0);
    assert(initproc != NULL && initproc->pid == 1);
}

// cpu_idle - at the end of kern_init, the first kernel thread idleproc will do below works
void
cpu_idle(void) {
    while (1) {
    	// idle线程执行逻辑就是不断的自旋循环，当发现存在有其它线程可以被调度时
    	// idle线程，即current.need_resched会被设置为真，之后便进行一次schedule线程调度
        if (current->need_resched) {
            schedule();
        }
    }
}

