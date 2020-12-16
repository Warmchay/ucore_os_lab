#include <stdio.h>
#include <proc.h>
#include <sem.h>
#include <monitor.h>
#include <assert.h>

#define N 5 /* 哲学家数目 */
#define LEFT (i-1+N)%N /* i的左邻号码 */
#define RIGHT (i+1)%N /* i的右邻号码 */
#define THINKING 0 /* 哲学家正在思考 */
#define HUNGRY 1 /* 哲学家想取得叉子 */
#define EATING 2 /* 哲学家正在吃面 */
#define TIMES  4 /* 吃4次饭 */
#define SLEEP_TIME 10

//-----------------philosopher problem using monitor ------------
/*PSEUDO CODE :philosopher problem using semaphore
system DINING_PHILOSOPHERS

VAR
me:    semaphore, initially 1;                    # for mutual exclusion 
s[5]:  semaphore s[5], initially 0;               # for synchronization 
pflag[5]: {THINK, HUNGRY, EAT}, initially THINK;  # philosopher flag 

# As before, each philosopher is an endless cycle of thinking and eating.

procedure philosopher(i)
  {
    while TRUE do
     {
       THINKING;
       take_chopsticks(i);
       EATING;
       drop_chopsticks(i);
     }
  }

# The take_chopsticks procedure involves checking the status of neighboring 
# philosophers and then declaring one's own intention to eat. This is a two-phase 
# protocol; first declaring the status HUNGRY, then going on to EAT.

procedure take_chopsticks(i)
  {
    DOWN(me);               # critical section 
    pflag[i] := HUNGRY;
    test[i];
    UP(me);                 # end critical section 
    DOWN(s[i])              # Eat if enabled 
   }

void test(i)                # Let phil[i] eat, if waiting 
  {
    if ( pflag[i] == HUNGRY
      && pflag[i-1] != EAT
      && pflag[i+1] != EAT)
       then
        {
          pflag[i] := EAT;
          UP(s[i])
         }
    }


# Once a philosopher finishes eating, all that remains is to relinquish the 
# resources---its two chopsticks---and thereby release waiting neighbors.

void drop_chopsticks(int i)
  {
    DOWN(me);                # critical section 
    test(i-1);               # Let phil. on left eat if possible 
    test(i+1);               # Let phil. on rght eat if possible 
    UP(me);                  # up critical section 
   }

*/


//---------- philosophers problem using semaphore ----------------------
int state_sema[N]; /* 记录每个人状态的数组 */
/* 信号量是一个特殊的整型变量 */
semaphore_t mutex; /* 临界区互斥 */
semaphore_t s[N]; /* 每个哲学家一个信号量 */

struct proc_struct *philosopher_proc_sema[N];

void phi_test_sema(i) /* i：哲学家号码从0到N-1 */
{ 
	// 当哲学家i处于饥饿状态(HUNGRY),且其左右临近的哲学家都没有在就餐状态(EATING)
    if(state_sema[i]==HUNGRY&&state_sema[LEFT]!=EATING
            &&state_sema[RIGHT]!=EATING)
    {
    	// 哲学家i饿了(HUNGRY)，且左右两边的叉子都没人用。
    	// 令哲学家进入就餐状态（EATING）
        state_sema[i]=EATING;
        // 唤醒阻塞在对应信号量上的哲学家线程
        up(&s[i]);
    }
}

void phi_take_forks_sema(int i) /* i：哲学家号码从0到N-1 */
{ 
	// 拿叉子时需要通过mutex信号量进行互斥，防止并发问题(进入临界区)
    down(&mutex);
    // 记录下哲学家i饥饿的事实(执行phi_take_forks_sema尝试拿叉子，说明哲学家i进入了HUNGRY饥饿状态)
    state_sema[i]=HUNGRY;
    // 试图同时得到左右两只叉子
    phi_test_sema(i);
    // 离开临界区(唤醒可能阻塞在mutex上的其它线程)
    up(&mutex);
    // phi_test_sema中如果成功拿到叉子进入了就餐状态，会先执行up(&s[i])，再执行down(&s[i])时便不会阻塞
    // 反之，如果phi_test_sema中没有拿到叉子，则down(&s[i])将会令哲学家i阻塞在信号量s[i]上
    down(&s[i]);
}

void phi_put_forks_sema(int i) /* i：哲学家号码从0到N-1 */
{ 
	// 放叉子时需要通过mutex信号量进行互斥，防止并发问题(进入临界区)
    down(&mutex); /* 进入临界区 */
    // 哲学家进餐结束(执行phi_put_forks_sema放下叉子，说明哲学家已经就餐完毕，重新进入THINKING思考状态)
    state_sema[i]=THINKING;
    // 当哲学家i就餐结束，放下叉子时。需要判断左、右临近的哲学家在自己就餐的这段时间内是否也进入了饥饿状态，却因为自己就餐拿走了叉子而无法同时获得左右两个叉子。
    // 为此哲学家i在放下叉子后需要尝试着判断在自己放下叉子后，左/右临近的、处于饥饿的哲学家能否进行就餐,如果可以就唤醒阻塞的哲学家线程，并令其进入就餐状态(EATING)
    phi_test_sema(LEFT); /* 看一下左邻居现在是否能进餐 */
    phi_test_sema(RIGHT); /* 看一下右邻居现在是否能进餐 */
    up(&mutex); /* 离开临界区q(唤醒可能阻塞在mutex上的其它线程) */
}

/**
 * 哲学家线程主体执行逻辑
 * */
int philosopher_using_semaphore(void * arg) /* i：哲学家号码，从0到N-1 */
{
    int i, iter=0;
    i=(int)arg;
    cprintf("I am No.%d philosopher_sema\n",i);
    while(iter++<TIMES)
    { /* 无限循环 */
        cprintf("Iter %d, No.%d philosopher_sema is thinking\n",iter,i); /* 哲学家正在思考 */
        // 使用休眠阻塞来模拟思考(哲学家线程阻塞N秒)
        do_sleep(SLEEP_TIME);
        // 哲学家尝试着去拿左右两边的叉子(如果没拿到会阻塞)
        phi_take_forks_sema(i); 
        /* 需要两只叉子，或者阻塞 */
        cprintf("Iter %d, No.%d philosopher_sema is eating\n",iter,i); /* 进餐 */
        // 使用休眠阻塞来模拟进餐(哲学家线程阻塞N秒)
        do_sleep(SLEEP_TIME);
        // 哲学家就餐结束，将叉子放回桌子。
        // 当发现之前有临近的哲学家尝试着拿左右叉子就餐时却没有成功拿到，尝试着唤醒对应的哲学家
        phi_put_forks_sema(i); 
        /* 把两把叉子同时放回桌子 */
    }
    cprintf("No.%d philosopher_sema quit\n",i);
    return 0;    
}

//-----------------philosopher problem using monitor ------------
/*PSEUDO CODE :philosopher problem using monitor
 * monitor dp
 * {
 *  enum {thinking, hungry, eating} state[5];
 *  condition self[5];
 *
 *  void pickup(int i) {
 *      state[i] = hungry;
 *      if ((state[(i+4)%5] != eating) && (state[(i+1)%5] != eating)) {
 *        state[i] = eating;
 *      else
 *         self[i].wait();
 *   }
 *
 *   void putdown(int i) {
 *      state[i] = thinking;
 *      if ((state[(i+4)%5] == hungry) && (state[(i+3)%5] != eating)) {
 *          state[(i+4)%5] = eating;
 *          self[(i+4)%5].signal();
 *      }
 *      if ((state[(i+1)%5] == hungry) && (state[(i+2)%5] != eating)) {
 *          state[(i+1)%5] = eating;
 *          self[(i+1)%5].signal();
 *      }
 *   }
 *
 *   void init() {
 *      for (int i = 0; i < 5; i++)
 *         state[i] = thinking;
 *   }
 * }
 */

struct proc_struct *philosopher_proc_condvar[N]; // N philosopher
int state_condvar[N];                            // the philosopher's state: EATING, HUNGARY, THINKING  
monitor_t mt, *mtp=&mt;                          // monitor

void phi_test_condvar (i) {
    // 当哲学家i处于饥饿状态(HUNGRY),且其左右临近的哲学家都没有在就餐状态(EATING)
    if(state_condvar[i]==HUNGRY&&state_condvar[LEFT]!=EATING
            &&state_condvar[RIGHT]!=EATING) {
        cprintf("phi_test_condvar: state_condvar[%d] will eating\n",i);
        // 哲学家i饿了(HUNGRY)，且左右两边的叉子都没人用。
    	// 令哲学家进入就餐状态（EATING）
        state_condvar[i] = EATING ;
        cprintf("phi_test_condvar: signal self_cv[%d] \n",i);
        // 唤醒阻塞在对应信号量上的哲学家线程
        cond_signal(&mtp->cv[i]) ;
    }
}


void phi_take_forks_condvar(int i) {
    // 拿叉子时需要通过mutex信号量进行互斥，防止并发问题(进入临界区)
    down(&(mtp->mutex));
//--------into routine in monitor--------------
    // LAB7 EXERCISE1: YOUR CODE
    // I am hungry
    // try to get fork
    // I am hungry
    // 记录下哲学家i饥饿的事实(执行phi_take_forks_condvar尝试拿叉子，说明哲学家i进入了HUNGRY饥饿状态)
    state_condvar[i]=HUNGRY;
    // 试图同时得到左右两只叉子
    phi_test_condvar(i);
    if (state_condvar[i] != EATING) {
    	// state_condvar[i]状态不为EATING，说明phi_test_condvar尝试拿左右叉子进餐失败
        cprintf("phi_take_forks_condvar: %d didn't get fork and will wait\n",i);
        // 等待阻塞在管程的条件变量cv[i]上
        cond_wait(&mtp->cv[i]);
    }
//--------leave routine in monitor--------------
    if(mtp->next_count>0){
    	// 当离开管程临界区时，如果发现存在线程等待在mtp->next上
    	// 在当前实验中，执行到这里的当前线程可能是阻塞在cond_wait中被其它线程唤醒的，对应线程是通过phi_test_condvar的cond_signal操作唤醒当前线程的
    	// 执行cond_signal时为了保证管程临界区内不存在并发的线程访问，在唤醒其它线程时，会把自己阻塞在管程的next信号量上，等待此时离开临界区的线程将其唤醒
        up(&(mtp->next));
    }else{
    	// 当离开管程临界区时,没有其它线程等待在mtp->next上，直接释放管程的互斥锁mutex即可(唤醒可能阻塞在mutex上的其它线程)
        up(&(mtp->mutex));
    }
}

void phi_put_forks_condvar(int i) {
	// 放叉子时需要通过mutex信号量进行互斥，防止并发问题(进入临界区)
    down(&(mtp->mutex));

//--------into routine in monitor--------------
    // LAB7 EXERCISE1: YOUR CODE
    // I ate over
    // test left and right neighbors
    // I ate over
    // 哲学家进餐结束(执行phi_put_forks_condvar放下叉子，说明哲学家已经就餐完毕，重新进入THINKING思考状态)
    state_condvar[i]=THINKING;
    // test left and right neighbors
    // 当哲学家i就餐结束，放下叉子时。需要判断左、右临近的哲学家在自己就餐的这段时间内是否也进入了饥饿状态，却因为自己就餐拿走了叉子而无法同时获得左右两个叉子。
    // 为此哲学家i在放下叉子后需要尝试着判断在自己放下叉子后，左/右临近的、处于饥饿的哲学家能否进行就餐,如果可以就唤醒阻塞的哲学家线程，并令其进入就餐状态(EATING)
    phi_test_condvar(LEFT); // 看一下左邻居现在是否能进餐
    phi_test_condvar(RIGHT); // 看一下右邻居现在是否能进餐
//--------leave routine in monitor--------------
    // lab7的参考答案
//    if(mtp->next_count>0){
//    	cprintf("execute here mtp->next_count>0 \n\n\n\n\n\n");
//        up(&(mtp->next));
//    }else{
//    	cprintf("execute here mtp->next_count=0 \n\n\n\n\n");
//        up(&(mtp->mutex));
//    }

    // 个人认为放叉子和取叉子的情况并不一样，不会出现mtp->next_count>0的情况，这里只需要释放互斥锁即可（如果这里理解的有问题，还请指正）
    // 当放叉子的线程在phi_put_forks_condvar中离开管程临界区时，只有两种情况
    // 1. 没有发现邻居可以进餐，自身不会被阻塞
    // 2. 发现有邻居之前被拿不到叉子阻塞了，现在可以进餐了，phi_test_condvar中的cond_signal会暂时令自己阻塞在next信号量上
    // 但是很快被自己叫醒的相邻的哲学家线程在被唤醒后一离开临界区就会将自己唤醒，在cond_signal被唤醒后的操作中mtp->next_count会自减，而变为0
    //
    // 以上两种情况下，由于管程本身最外面有一个mutex互斥信号量，所以不会出现两个线程同时阻塞在next信号量中，因此也就不会出现参考答案中mtp->next_count>0的情况
    up(&(mtp->mutex));
}

/**
 * 哲学家线程执行主体逻辑(条件变量实现)
 * */
//---------- philosophers using monitor (condition variable) ----------------------
int philosopher_using_condvar(void * arg) { /* arg is the No. of philosopher 0~N-1*/
  
    int i, iter=0;
    i=(int)arg;
    cprintf("I am No.%d philosopher_condvar\n",i);
    while(iter++<TIMES)
    { /* iterate*/
        cprintf("Iter %d, No.%d philosopher_condvar is thinking\n",iter,i); /* 哲学家正在思考*/
        // 使用休眠阻塞来模拟思考(哲学家线程阻塞N秒)
        do_sleep(SLEEP_TIME);
        // 哲学家尝试着去拿左右两边的叉子(如果没拿到会阻塞)
        phi_take_forks_condvar(i); 
        /* 需要两只叉子，或者阻塞 */
        cprintf("Iter %d, No.%d philosopher_condvar is eating\n",iter,i); /* 进餐 */
        // 使用休眠阻塞来模拟进餐(哲学家线程阻塞N秒)
        do_sleep(SLEEP_TIME);
        // 哲学家就餐结束，将叉子放回桌子。
        // 当发现之前有临近的哲学家尝试着拿左右叉子就餐时却没有成功拿到，尝试着唤醒对应的哲学家
        phi_put_forks_condvar(i); 
        /* 把两把叉子同时放回桌子 */
    }
    cprintf("No.%d philosopher_condvar quit\n",i);
    return 0;    
}

void check_sync(void){

    int i;

    //check semaphore
    sem_init(&mutex, 1);
    for(i=0;i<N;i++){
        sem_init(&s[i], 0);
        int pid = kernel_thread(philosopher_using_semaphore, (void *)i, 0);
        if (pid <= 0) {
            panic("create No.%d philosopher_using_semaphore failed.\n");
        }
        philosopher_proc_sema[i] = find_proc(pid);
        set_proc_name(philosopher_proc_sema[i], "philosopher_sema_proc");
    }

    //check condition variable
    monitor_init(&mt, N);
    for(i=0;i<N;i++){
        state_condvar[i]=THINKING;
        int pid = kernel_thread(philosopher_using_condvar, (void *)i, 0);
        if (pid <= 0) {
            panic("create No.%d philosopher_using_condvar failed.\n");
        }
        philosopher_proc_condvar[i] = find_proc(pid);
        set_proc_name(philosopher_proc_condvar[i], "philosopher_condvar_proc");
    }
}
