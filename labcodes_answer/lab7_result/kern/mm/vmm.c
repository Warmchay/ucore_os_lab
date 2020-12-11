#include <vmm.h>
#include <sync.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <error.h>
#include <pmm.h>
#include <x86.h>
#include <swap.h>
#include <kmalloc.h>

/* 
  vmm design include two parts: mm_struct (mm) & vma_struct (vma)
  mm is the memory manager for the set of continuous virtual memory  
  area which have the same PDT. vma is a continuous virtual memory area.
  There a linear link list for vma & a redblack link list for vma in mm.
---------------
  mm related functions:
   golbal functions
     struct mm_struct * mm_create(void)
     void mm_destroy(struct mm_struct *mm)
     int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr)
--------------
  vma related functions:
   global functions
     struct vma_struct * vma_create (uintptr_t vm_start, uintptr_t vm_end,...)
     void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma)
     struct vma_struct * find_vma(struct mm_struct *mm, uintptr_t addr)
   local functions
     inline void check_vma_overlap(struct vma_struct *prev, struct vma_struct *next)
---------------
   check correctness functions
     void check_vmm(void);
     void check_vma_struct(void);
     void check_pgfault(void);
*/

static void check_vmm(void);
static void check_vma_struct(void);
static void check_pgfault(void);

// mm_create -  alloc a mm_struct & initialize it.
struct mm_struct *
mm_create(void) {
	// 使用kmalloc分配对应的物理空间
    struct mm_struct *mm = kmalloc(sizeof(struct mm_struct));

    // 判断是否申请分配是否成功
    if (mm != NULL) {
    	// 初始化mm_struct的属性
        list_init(&(mm->mmap_list));
        mm->mmap_cache = NULL;
        mm->pgdir = NULL;
        mm->map_count = 0;
        // 将mm设置进全局虚拟内存页替换管理器swap_manager
        if (swap_init_ok) swap_init_mm(mm);
        else mm->sm_priv = NULL;
        
        set_mm_count(mm, 0);
        sem_init(&(mm->mm_sem), 1);
    }    
    return mm;
}

// vma_create - alloc a vma_struct & initialize it. (addr range: vm_start~vm_end)
struct vma_struct *
vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags) {
	// 分配vma_struct所需的物理内存，返回指向其空间的vma_struct指针
    struct vma_struct *vma = kmalloc(sizeof(struct vma_struct));

    if (vma != NULL) {
    	// 初始化vma的属性
    	vma->vm_start = vm_start;
        vma->vm_end = vm_end;
        vma->vm_flags = vm_flags;
    }
    return vma;
}


// find_vma - find a vma  (vma->vm_start <= addr <= vma_vm_end)
// 从参数mm结构中的vma块链表，查找addr是否是一个合法的虚拟内存地址
struct vma_struct *
find_vma(struct mm_struct *mm, uintptr_t addr) {
    struct vma_struct *vma = NULL;
    if (mm != NULL) {
    	// 先从mmap_cache缓存中尝试查询
        vma = mm->mmap_cache;
        // 判断从cache中获取到的是否满足条件
        if (!(vma != NULL && vma->vm_start <= addr && vma->vm_end > addr)) {
        		// cache中的vma块不匹配
                bool found = 0;
                list_entry_t *list = &(mm->mmap_list), *le = list;
                // 迭代mm->mmap_list中的每个节点
                while ((le = list_next(le)) != list) {
                	// 将vma链表节点转为vma
                    vma = le2vma(le, list_link);
                    if (vma->vm_start<=addr && addr < vma->vm_end) {
                    	// 判断addr是否在当前vma的映射范围内
                        found = 1;
                        // 找到了，跳出循环
                        break;
                    }
                }
                if (!found) {
                	// 没找到返回null
                    vma = NULL;
                }
        }
        if (vma != NULL) {
        	// 找到了addr对应的vma块，用其刷新mmap_cache
            mm->mmap_cache = vma;
        }
    }
    return vma;
}


// check_vma_overlap - check if vma1 overlaps vma2 ?
static inline void
check_vma_overlap(struct vma_struct *prev, struct vma_struct *next) {
    assert(prev->vm_start < prev->vm_end);
    assert(prev->vm_end <= next->vm_start);
    assert(next->vm_start < next->vm_end);
}


// insert_vma_struct -insert vma in mm's list link
// 将@vma按照指定规则插入进@mm的mm->mmap_list中
void
insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma) {
    assert(vma->vm_start < vma->vm_end);
    list_entry_t *list = &(mm->mmap_list);
    list_entry_t *le_prev = list, *le_next;

    list_entry_t *le = list;
    // 迭代mm->mmap_list中的每一个节点
    while ((le = list_next(le)) != list) {
        struct vma_struct *mmap_prev = le2vma(le, list_link);
        // 找到节点中起始地址距离vma参数起始地址最近的一个节点
        if (mmap_prev->vm_start > vma->vm_start) {
            break;
        }
        le_prev = le;
    }

    // 找到恰好位于参数vma映射空间之前(le_prev)和之后的节点(le_next)
    le_next = list_next(le_prev);

    /* check overlap */
    // 检验是否有le_prev和vma的映射地址区间是否有重叠
    if (le_prev != list) {
        check_vma_overlap(le2vma(le_prev, list_link), vma);
    }
    // 检验是否有le_next和vma的映射地址区间是否有重叠
    if (le_next != list) {
        check_vma_overlap(vma, le2vma(le_next, list_link));
    }

    // 令vma->vm_mm指向mm
    vma->vm_mm = mm;
    // 将参数vma的节点插入找到的le_prev之后，同时也是le_next之前
    list_add_after(le_prev, &(vma->list_link));
    // mm包含的vma块数量自增1
    mm->map_count ++;
}

// mm_destroy - free mm and mm internal fields
void
mm_destroy(struct mm_struct *mm) {
    assert(mm_count(mm) == 0);

    list_entry_t *list = &(mm->mmap_list), *le;
    // 遍历mm->mmap_list中的每一个节点
    while ((le = list_next(list)) != list) {
    	// 将其从mm->mmap_list中移除
        list_del(le);
        // 并释放vma所占用的物理内存空间
        kfree(le2vma(le, list_link));  //kfree vma        
    }
    // 释放mm所占用的内存空间
    kfree(mm); //kfree mm
    // 令mm指向null
    mm=NULL;
}

/**
 * 在mm中设置，进行vma的映射
 * 创建一个从addr起始到addr+len截止的vma结构，插入mm->mmap_list中
 * */
int
mm_map(struct mm_struct *mm, uintptr_t addr, size_t len, uint32_t vm_flags,
       struct vma_struct **vma_store) {
    uintptr_t start = ROUNDDOWN(addr, PGSIZE), end = ROUNDUP(addr + len, PGSIZE);
    if (!USER_ACCESS(start, end)) {
        return -E_INVAL;
    }

    assert(mm != NULL);

    int ret = -E_INVAL;

    struct vma_struct *vma;
    if ((vma = find_vma(mm, start)) != NULL && end > vma->vm_start) {
    	// 当前mm中已经有对应的映射关系了，直接返回
        goto out;
    }
    ret = -E_NO_MEM;

    // 创建一个新的vma结构(start<->end段)
    if ((vma = vma_create(start, end, vm_flags)) == NULL) {
        goto out;
    }
    // 将新创建的vma插入mm->mmap_list中
    insert_vma_struct(mm, vma);
    if (vma_store != NULL) {
    	// 参数vma_store存在，则设置为新的vma为返回值
        *vma_store = vma;
    }
    ret = 0;

out:
    return ret;
}

/**
 * 将mm_struct *from中的内存管理相关的数据复制到struct mm_struct *to中
 * 1. 复制mm_struct.mmap_list（合法的虚拟空间段链表）
 * 2. 对mmap_list中对应虚拟地址空间内的每个页表项进行复制
 * */
int
dup_mmap(struct mm_struct *to, struct mm_struct *from) {
    assert(to != NULL && from != NULL);
    // 获得原始线程from的连续虚拟内存块链表mmap_list
    list_entry_t *list = &(from->mmap_list), *le = list;
    while ((le = list_prev(le)) != list) {
        struct vma_struct *vma, *nvma;
        // 迭代from连续虚拟内存块链表的每个节点，获得对应的vma结构
        vma = le2vma(le, list_link);
        // 按照同样的虚拟地址起始、截止范围和vm_flags,创建一份一样的vma结构(new vma)
        nvma = vma_create(vma->vm_start, vma->vm_end, vma->vm_flags);
        if (nvma == NULL) {
            return -E_NO_MEM;
        }
        // 将新创建的nvma插入新线程to的连续虚拟内存块链表mmap_list之中
        insert_vma_struct(to, nvma);

        bool share = 0;
        // 非共享内存，进行vm_start->vm_end这一虚拟空间段页表内容的复制
        if (copy_range(to->pgdir, from->pgdir, vma->vm_start, vma->vm_end, share) != 0) {
            return -E_NO_MEM;
        }
    }
    return 0;
}

/**
 * 解除mm对应一级页表、二级页表的所有虚实映射关系
 * */
void
exit_mmap(struct mm_struct *mm) {
    assert(mm != NULL && mm_count(mm) == 0);
    pde_t *pgdir = mm->pgdir;
    list_entry_t *list = &(mm->mmap_list), *le = list;
    while ((le = list_next(le)) != list) {
        struct vma_struct *vma = le2vma(le, list_link);
        unmap_range(pgdir, vma->vm_start, vma->vm_end);
    }
    while ((le = list_next(le)) != list) {
        struct vma_struct *vma = le2vma(le, list_link);
        exit_range(pgdir, vma->vm_start, vma->vm_end);
    }
}

bool
copy_from_user(struct mm_struct *mm, void *dst, const void *src, size_t len, bool writable) {
    if (!user_mem_check(mm, (uintptr_t)src, len, writable)) {
        return 0;
    }
    memcpy(dst, src, len);
    return 1;
}

bool
copy_to_user(struct mm_struct *mm, void *dst, const void *src, size_t len) {
    if (!user_mem_check(mm, (uintptr_t)dst, len, 1)) {
        return 0;
    }
    memcpy(dst, src, len);
    return 1;
}

// vmm_init - initialize virtual memory management
//          - now just call check_vmm to check correctness of vmm
void
vmm_init(void) {
    check_vmm();
}

// check_vmm - check correctness of vmm
static void
check_vmm(void) {
    size_t nr_free_pages_store = nr_free_pages();
    
    check_vma_struct();
    check_pgfault();

//    assert(nr_free_pages_store == nr_free_pages());

    cprintf("check_vmm() succeeded.\n");
}

static void
check_vma_struct(void) {
	// 获得检查前的 当前空闲物理页数
    size_t nr_free_pages_store = nr_free_pages();

    // 创建物理管理器
    struct mm_struct *mm = mm_create();
    assert(mm != NULL);

    int step1 = 10, step2 = step1 * 10;

    int i;
    for (i = step1; i >= 1; i --) {
    	// 创建step1个vma页块(虚拟地址空间从小到大增长)
        struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);
        // 将当前vma插入mm中的vma链表
        insert_vma_struct(mm, vma);
    }

    for (i = step1 + 1; i <= step2; i ++) {
    	// 创建step2个vma页块(虚拟地址空间从小到大增长)
        struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);
        // 将当前vma插入mm中的vma链表
        insert_vma_struct(mm, vma);
    }

    // 遍历mm的vma链表
    list_entry_t *le = list_next(&(mm->mmap_list));

    for (i = 1; i <= step2; i ++) {
        assert(le != &(mm->mmap_list));
        struct vma_struct *mmap = le2vma(le, list_link);
        assert(mmap->vm_start == i * 5 && mmap->vm_end == i * 5 + 2);
        le = list_next(le);
    }

    for (i = 5; i <= 5 * step2; i +=5) {
        struct vma_struct *vma1 = find_vma(mm, i);
        assert(vma1 != NULL);
        struct vma_struct *vma2 = find_vma(mm, i+1);
        assert(vma2 != NULL);
        struct vma_struct *vma3 = find_vma(mm, i+2);
        assert(vma3 == NULL);
        struct vma_struct *vma4 = find_vma(mm, i+3);
        assert(vma4 == NULL);
        struct vma_struct *vma5 = find_vma(mm, i+4);
        assert(vma5 == NULL);

        assert(vma1->vm_start == i  && vma1->vm_end == i  + 2);
        assert(vma2->vm_start == i  && vma2->vm_end == i  + 2);
    }

    for (i =4; i>=0; i--) {
    	// 检验find_vma的正确性
        struct vma_struct *vma_below_5= find_vma(mm,i);
        if (vma_below_5 != NULL ) {
           cprintf("vma_below_5: i %x, start %x, end %x\n",i, vma_below_5->vm_start, vma_below_5->vm_end); 
        }
        assert(vma_below_5 == NULL);
    }

    // 释放mm结构
    mm_destroy(mm);

//    assert(nr_free_pages_store == nr_free_pages());

    cprintf("check_vma_struct() succeeded!\n");
}

struct mm_struct *check_mm_struct;

// check_pgfault - check correctness of pgfault handler 检查缺页异常处理器的正确性
static void
check_pgfault(void) {
	// 获得检查前的 当前空闲物理页数
    size_t nr_free_pages_store = nr_free_pages();

    // 创建物理管理器
    check_mm_struct = mm_create();
    assert(check_mm_struct != NULL);

    struct mm_struct *mm = check_mm_struct;
    // 设置mm的页表为内核页表
    pde_t *pgdir = mm->pgdir = boot_pgdir;
    assert(pgdir[0] == 0);

    // 创建一个合法映射0~PTSIZE虚拟地址空间的vma块
    struct vma_struct *vma = vma_create(0, PTSIZE, VM_WRITE);
    assert(vma != NULL);

    // 将上面的vma放入mm中
    insert_vma_struct(mm, vma);

    uintptr_t addr = 0x100;
    assert(find_vma(mm, addr) == vma);

    int i, sum = 0;
    for (i = 0; i < 100; i ++) {
    	// 通过迭代反复的自增指针，访问对应的虚拟地址
        *(char *)(addr + i) = i;
        sum += i;
    }
    for (i = 0; i < 100; i ++) {
    	// 通过迭代反复的自增指针，访问对应的虚拟地址
        sum -= *(char *)(addr + i);
    }
    // 判断sum在连续递增再连续递减之后，是否依然为初始值0
    assert(sum == 0);

    page_remove(pgdir, ROUNDDOWN(addr, PGSIZE));
    free_page(pde2page(pgdir[0]));
    pgdir[0] = 0;

    mm->pgdir = NULL;
    mm_destroy(mm);
    check_mm_struct = NULL;

    assert(nr_free_pages_store == nr_free_pages());

    cprintf("check_pgfault() succeeded!\n");
}
//page fault number
volatile unsigned int pgfault_num=0;

/* do_pgfault - interrupt handler to process the page fault execption
 * 				缺页异常中断处理器
 * @mm         : the control struct for a set of vma using the same PDT
 * 				当前进程的mm总内存管理器
 * @error_code : the error code recorded in trapframe->tf_err which is setted by x86 hardware
 *				x86硬件在中断发生时，设置进中断栈帧的中断错误码
 * @addr       : the addr which causes a memory access exception, (the contents of the CR2 register)
 *				引起内存访问异常的线性地址(中断异常发生时，保存在CR2寄存器中)
 * CALL GRAPH: trap--> trap_dispatch-->pgfault_handler-->do_pgfault
 * The processor provides ucore's do_pgfault function with two items of information to aid in diagnosing
 * the exception and recovering from it.
 *   (1) The contents of the CR2 register. The processor loads the CR2 register with the
 *       32-bit linear address that generated the exception. The do_pgfault fun can
 *       use this address to locate the corresponding page directory and page-table
 *       entries.
 *   (2) An error code on the kernel stack. The error code for a page fault has a format different from
 *       that for other exceptions. The error code tells the exception handler three things:
 *         -- The P flag   (bit 0) indicates whether the exception was due to a not-present page (0)
 *            or to either an access rights violation or the use of a reserved bit (1).
 *         -- The W/R flag (bit 1) indicates whether the memory access that caused the exception
 *            was a read (0) or write (1).
 *         -- The U/S flag (bit 2) indicates whether the processor was executing at user mode (1)
 *            or supervisor mode (0) at the time of the exception.
 */
int
do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr) {
    int ret = -E_INVAL;
    //try to find a vma which include addr
    // 试图从mm关联的vma链表块中查询，是否存在当前addr线性地址匹配的vma块
    struct vma_struct *vma = find_vma(mm, addr);

    // 全局页异常处理数自增1
    pgfault_num++;
    //If the addr is in the range of a mm's vma?
    if (vma == NULL || vma->vm_start > addr) {
    	// 如果没有匹配到vma
        cprintf("not valid addr %x, and  can not find it in vma\n", addr);
        goto failed;
    }
    //check the error_code
    // 页访问异常错误码有32位。位0为1 表示对应物理页不存在；位1为1 表示写异常(比如写了只读页)；位2为1 表示访问权限异常（比如用户态程序访问内核空间的数据）
    // 对3求模，主要判断bit0、bit1的值
    switch (error_code & 3) {
    default:
            /* error code flag : default is 3 ( W/R=1, P=1): write, present */
    	// bit0，bit1都为1，访问的映射页表项存在，且发生的是写异常
    	// 说明发生了缺页异常
    case 2: /* error code flag : (W/R=1, P=0): write, not present */
    	// bit0为0，bit1为1，访问的映射页表项不存在、且发生的是写异常
        if (!(vma->vm_flags & VM_WRITE)) {
        	// 对应的vma块映射的虚拟内存空间是不可写的,权限校验失败
            cprintf("do_pgfault failed: error code flag = write AND not present, but the addr's vma cannot write\n");
            // 跳转failed直接返回
            goto failed;
        }
        // 校验通过，则说明发生了缺页异常
        break;
    case 1: /* error code flag : (W/R=0, P=1): read, present */
    	// bit0为1，bit1为0，访问的映射页表项存在，且发生的是读异常(可能是访问权限异常)
        cprintf("do_pgfault failed: error code flag = read AND present\n");
        // 跳转failed直接返回
        goto failed;
    case 0: /* error code flag : (W/R=0, P=0): read, not present */
    	// bit0为0，bit1为0，访问的映射页表项不存在，且发生的是读异常
        if (!(vma->vm_flags & (VM_READ | VM_EXEC))) {
        	// 对应的vma映射的虚拟内存空间是不可读且不可执行的
            cprintf("do_pgfault failed: error code flag = read AND not present, but the addr's vma cannot read or exec\n");
            goto failed;
        }
        // 校验通过，则说明发生了缺页异常
    }
    /* IF (write an existed addr ) OR
     *    (write an non_existed addr && addr is writable) OR
     *    (read  an non_existed addr && addr is readable)
     * THEN
     *    continue process
     */

    // 构造需要设置的缺页页表项的perm权限
    uint32_t perm = PTE_U;
    if (vma->vm_flags & VM_WRITE) {
        perm |= PTE_W;
    }
    // 构造需要设置的缺页页表项的线性地址(按照PGSIZE向下取整，进行页面对齐)
    addr = ROUNDDOWN(addr, PGSIZE);

    ret = -E_NO_MEM;

    // 用于映射的页表项指针（page table entry, pte）
    pte_t *ptep=NULL;
    /*LAB3 EXERCISE 1: YOUR CODE
    * Maybe you want help comment, BELOW comments can help you finish the code
    *
    * Some Useful MACROs and DEFINEs, you can use them in below implementation.
    * MACROs or Functions:
    *   get_pte : get an pte and return the kernel virtual address of this pte for la
    *             if the PT contians this pte didn't exist, alloc a page for PT (notice the 3th parameter '1')
    *   pgdir_alloc_page : call alloc_page & page_insert functions to allocate a page size memory & setup
    *             an addr map pa<--->la with linear address la and the PDT pgdir
    * DEFINES:
    *   VM_WRITE  : If vma->vm_flags & VM_WRITE == 1/0, then the vma is writable/non writable
    *   PTE_W           0x002                   // page table/directory entry flags bit : Writeable
    *   PTE_U           0x004                   // page table/directory entry flags bit : User can access
    * VARIABLES:
    *   mm->pgdir : the PDT of these vma
    *
    */
#if 0
    /*LAB3 EXERCISE 1: YOUR CODE*/
    ptep = ???              //(1) try to find a pte, if pte's PT(Page Table) isn't existed, then create a PT.
    if (*ptep == 0) {
                            //(2) if the phy addr isn't exist, then alloc a page & map the phy addr with logical addr

    }
    else {
    /*LAB3 EXERCISE 2: YOUR CODE
    * Now we think this pte is a  swap entry, we should load data from disk to a page with phy addr,
    * and map the phy addr with logical addr, trigger swap manager to record the access situation of this page.
    *
    *  Some Useful MACROs and DEFINEs, you can use them in below implementation.
    *  MACROs or Functions:
    *    swap_in(mm, addr, &page) : alloc a memory page, then according to the swap entry in PTE for addr,
    *                               find the addr of disk page, read the content of disk page into this memroy page
    *    page_insert ： build the map of phy addr of an Page with the linear addr la
    *    swap_map_swappable ： set the page swappable
    */
    /*
     * LAB5 CHALLENGE ( the implmentation Copy on Write)
		There are 2 situlations when code comes here.
		  1) *ptep & PTE_P == 1, it means one process try to write a readonly page. 
		     If the vma includes this addr is writable, then we can set the page writable by rewrite the *ptep.
		     This method could be used to implement the Copy on Write (COW) thchnology(a fast fork process method).
		  2) *ptep & PTE_P == 0 & but *ptep!=0, it means this pte is a  swap entry.
		     We should add the LAB3's results here.
     */
        if(swap_init_ok) {
            struct Page *page=NULL;
                                    //(1）According to the mm AND addr, try to load the content of right disk page
                                    //    into the memory which page managed.
                                    //(2) According to the mm, addr AND page, setup the map of phy addr <---> logical addr
                                    //(3) make the page swappable.
                                    //(4) [NOTICE]: you myabe need to update your lab3's implementation for LAB5's normal execution.
        }
        else {
            cprintf("no swap_init_ok but ptep is %x, failed\n",*ptep);
            goto failed;
        }
   }
#endif
    // try to find a pte, if pte's PT(Page Table) isn't existed, then create a PT.
    // (notice the 3th parameter '1')
    // 获取addr线性地址在mm所关联页表中的页表项
    // 第三个参数=1 表示如果对应页表项不存在，则需要新创建这个页表项
    if ((ptep = get_pte(mm->pgdir, addr, 1)) == NULL) {
        cprintf("get_pte in do_pgfault failed\n");
        goto failed;
    }
    
    // 如果对应页表项的内容每一位都全为0，说明之前并不存在，需要设置对应的数据，进行线性地址与物理地址的映射
    if (*ptep == 0) { // if the phy addr isn't exist, then alloc a page & map the phy addr with logical addr
    	// 令pgdir指向的页表中，la线性地址对应的二级页表项与一个新分配的物理页Page进行虚实地址的映射
        if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL) {
            cprintf("pgdir_alloc_page in do_pgfault failed\n");
            goto failed;
        }
    }
    else {
        struct Page *page=NULL;
        cprintf("do pgfault: ptep %x, pte %x\n",ptep, *ptep);
        if (*ptep & PTE_P) {
            //if process write to this existed readonly page (PTE_P means existed), then should be here now.
            //we can implement the delayed memory space copy for fork child process (AKA copy on write, COW).
            //we didn't implement now, we will do it in future.
            panic("error write a non-writable pte");
            //page = pte2page(*ptep);
        } else{
           // if this pte is a swap entry, then load data from disk to a page with phy addr
           // and call page_insert to map the phy addr with logical addr
           // 如果不是全为0，说明可能是之前被交换到了swap磁盘中
           if(swap_init_ok) { 
               // 将addr线性地址对应的物理页数据从磁盘交换到物理内存中(令Page指针指向交换成功后的物理页)
               if ((ret = swap_in(mm, addr, &page)) != 0) {
                   // swap_in返回值不为0，表示换入失败
                   cprintf("swap_in in do_pgfault failed\n");
                   goto failed;
               }    

           }  
           else {
           	   // 如果没有开启swap磁盘虚拟内存交换机制，但是却执行至此，则出现了问题
               cprintf("no swap_init_ok but ptep is %x, failed\n",*ptep);
               goto failed;
           }
       } 
       // 将交换进来的page页与mm->padir页表中对应addr的二级页表项建立映射关系(perm标识这个二级页表的各个权限位)
       page_insert(mm->pgdir, page, addr, perm);
       // 当前page是为可交换的，将其加入全局虚拟内存交换管理器的管理
       swap_map_swappable(mm, addr, page, 1);
       page->pra_vaddr = addr;
   }
   // 返回0代表缺页异常处理成功
   ret = 0;
failed:
    return ret;
}

bool
user_mem_check(struct mm_struct *mm, uintptr_t addr, size_t len, bool write) {
    if (mm != NULL) {
        if (!USER_ACCESS(addr, addr + len)) {
            return 0;
        }
        struct vma_struct *vma;
        uintptr_t start = addr, end = addr + len;
        while (start < end) {
            if ((vma = find_vma(mm, start)) == NULL || start < vma->vm_start) {
                return 0;
            }
            if (!(vma->vm_flags & ((write) ? VM_WRITE : VM_READ))) {
                return 0;
            }
            if (write && (vma->vm_flags & VM_STACK)) {
                if (start < vma->vm_start + PGSIZE) { //check stack start & size
                    return 0;
                }
            }
            start = vma->vm_end;
        }
        return 1;
    }
    return KERN_ACCESS(addr, addr + len);
}

