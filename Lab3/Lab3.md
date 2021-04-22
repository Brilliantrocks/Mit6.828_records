# Lab3 用户环境
## 介绍
这个实验中要实现基础的内核功能要求来让用户模式环境运行。通过设置数据结构加强JOS内核来追踪用户环境，创建一个单一用户环境，加载一个程序镜像到里面并运行。同时也要使JOS内核有能力处理所有用户环境的系统调用和它引起的异常。  
  
注意，本实验中环境和进程这两个词是可以互换的-都指一个运行你运行程序的抽象。用环境而不是进程来强调JOS环境和UNIX进程提供了不同的交互，而不是同一个语义。  
  
Lab3 包含一系列新文件：  
```
inc/ 	env.h 	    用户模式环境的共有定义
	    trap.h 	    系统陷入处理共有定义
	    syscall.h 	用户环境向内核系统调用的共有定义
	    lib.h 	    用户模式支持库共有定义
kern/ 	env.h 	    内核的用户模式环境私有定义
	    env.c 	    内核代码实现的用户模式环境
	    trap.h      内核的陷入处理私有定义
	    trap.c 	    陷入处理代码
	    trapentry.S 	汇编语言陷入处理器进入点
	    syscall.h 	内核的系统调用处理私有定义
	    syscall.c 	系统调用实现代码
lib/ 	Makefrag 	Makefile框架来 build user-mode library, obj/lib/libjos.a
	    entry.S 	用户环境的汇编语言进入点
	    libmain.c 	entry.s调用的用户模式库设置代码
	    syscall.c 	用户模式系统调用根函数
	    console.c 	用户模式下putchar,getchar的实现，提供控制台I/o
 	    exit.c 	    用户模式下exit的实现
	    panic.c 	用户模式下panic的实现
user/ 	* 	        集中测试内核lab3代码的程序  
```  
### 行内汇编
本实验中GCC的行内汇编语言功能会很有用，尽管不使用它完成实验也是可能的。至少要能够理解源文件已存在的行内汇编语言的框架（“asm”陈述句）。[参考材料]（https://pdos.csail.mit.edu/6.828/2018/reference.html）中有多种关于GCC行内汇编的信息。  
  
## Part a 用户环境和异常处理
新的inc/env.h文件包含了基础的JOS用户环境定义。内核使用Env数据结构来保持对每个用户环境的追踪。本实验初步创建一个环境，但需要设计JOS内核来支持多环境；lab4将充分利用这一功能来让一个用户环境fork其他的环境。  
  
阅读/kern/env.c，内核维护了三个主要的适用于环境的全局变量：  
```
struct Env *envs = NULL;		// 所有环境
struct Env *curenv = NULL;		// 当前环境
static struct Env *env_free_list;	// 空闲环境列表
```  
一旦JOS启动并运行，envs指针指向一个Env结构的数组，代表系统中所有的环境。在设计中，内核将同时支持多达NENV的活动环境，尽管通常运行的环境数远少于此（NENV是定义在inc/env.h中的常数)。一旦它被分配了，envs数组将包含NENV个可能的环境的一个Env数据结构实例。  
  
JOS内核保存所有的非活动Env到env_free_list。这个设计允许简单地分配和回收环境，仅仅只要将它们从表中添加和移除。  
  
内核使用curenv符号来追踪当前正在执行地环境。在启动阶段，第一个环境运行前，curenv被初始化为NULL。  
  
### 环境状态
Env结构定义在inc/env.h中，如下：(未来实验中会添加更多字段)  
```
struct Env {
	struct Trapframe env_tf;	// 保存地寄存器
	struct Env *env_link;		// 下个空闲进程
	envid_t env_id;			// 唯一环境编号
	envid_t env_parent_id;		// 父环境的id
	enum EnvType env_type;		// 提示特定的系统环境
	unsigned env_status;		// 环境状态
	uint32_t env_runs;		// 环境已经运行的次数

	// 地址空间
	pde_t *env_pgdir;		// 内核页目录虚拟地址
};
```
  
**env_tf**:这个结构定义在inc/trap.h中，在环境不在运行时保存了环境的寄存器的值：例如，当内核或不同的环境正在运行时。内核在从用户模式切换到内核模式时保存这些值，使得环境之后可以从它离开的地方继续运行。  
  
**env_link**:这是指向env_free_list.env中下一个Env的指针。/env_free_list指向表中的第一个空闲环境。  
  
**env_id**：内存在这里存放一个能唯一标识当前正在使用这个Env结构的环境的值（使用env数组中的这一个槽位）。在用户环境终结后，内核可能会重新分配同样的Env结构到一个不同的环境-但新环境将有一个和之前环境不同的env_id，尽管它在重用env数组中相同的槽位。  
  
**env_parent_id**:内核存放创建当前环境的那个环境的env_id。通过这种方式环境可以形成一个“族谱”，在决定谁能对环境做什么是安全的时会很有用。  
  
**env_type**:它被用来区分特定的环境。对于大多数环境，它会是ENV_TYPE_USER。之后的实验里会介绍几种特别系统服务环境的额外类型。  
  
**env_status**： 
这个变量保存了下列值中的一个：  
ENV_FREE:提示这个Env结构是非活动的，在env_free_list里。  
ENV_RUNNABLE:提示这个Env结构代表了一个等待处理器运行的环境。  
ENV_RUNNING:提示这个Env结果代表了当前正在运行的环境。  
ENV_NOT_RUNNABLE:提示这个Env结构代表了当前活动的环境，但它现在还没准备好运行：例如，因为它在等待来自另一个环境的跨进程通信（IPC)。  
ENV_DYING:提示这个Env结构代表了也该僵尸环境。一个僵尸环境将在下次陷入内核时释放。在Lab4前不会用到这个标志位。  
**env_pgdir**:这个变量保持了这个环境的页目录在内核的虚拟地址。  
  
就像一个Unix进程，一个JOS环境将线程和地址空间的概念对应起来。线程主要被保存的寄存器（env_tf）定义，地址空间被env_pgdir指向的页目录和页表定义。为了运行一个环境，CPU必须正确设置保存寄存器和地址空间。  
  
strut env相似于xv6中的struct proc。两者都有环境（进程）的用户模式寄存器状态结构Trapframe。在JOS里，独立环境没有它们自己的的内核栈，不像xv6的进程。同一时间只能有一个JOS环境在内核里，所以JOS只需要一个内核栈。  
  
### 分配环境数组
在lab2中曾在mem_init()里为pages[]数组分配内存，它时一个内核用来保持追踪页空闲的表。现在进一步修改mem_init来分配一个相似的Env结构envs[]。  
> + Exercise 1
    修改kern/pmap.c中的mem_init()来分配和映射envs数组。这个数组有NENV个Env结构实例组成，就像pages[]。同样的，envs的内存应该被映射为用户只读到UENVS（见inc/memlayout.h)使得用户进程可以读取这个数组。
    确保代码可以通过check_kern_ogdir()  
  
envs数组：
`envs = (struct Env*)boot_alloc(sizeof(struct Env)*NENV);`  
`memset(envs, 0, sizeof(struct Env)*NENV);` 
内存映射：  
`boot_map_region(kern_pgdir, UENVS, PTSIZE, PADDR(envs), PTE_U);`
  
### 创建并运行环境 
现在在kern/env.c中编写运行用户环境的必要代码。因为还没有文件系统，要设置内核来加载一个嵌入在内核内部的静态二进制镜像。JOS把这个二进制文件作为ELF可执行镜像嵌入到内核里。  
  
Lab3的GNUmakefile生成一系列二进制映像到obj/user/目录下。在kern/Makefrag里可以注意到把这些二进制文件当作.o文件直接链接到内核可执行的把戏，而不是作为编译产生的常规.o文件。(只要链接器关注，这些文件不必是都是ELF文件-它们可以是任何东西，如文本文件或者图片。）在构建内核后，查阅boj/kern/kernel.sym，可以发现链接器魔法般地产生了一系列有着晦涩名字地有趣地符号如_binary_obj_user_hello_start， _binary_obj_user_hello_end，和 _binary_obj_user_hello_size 。链接器压制二进制文件名产生这些符号名；这些符号向常规内核代码提供了一直引用嵌入二进制文件地方法。  
  
在kern/init.c的i386_init()中，可以发现在一个环境中运行其中一个二进制镜像的代码。然而，设置用户环境的关键代码还没有完成：  
  
> + Exercise2 在文件env.c中完成下列函数：
    env_init()
    初始化所有envs数组中的Env结构并将它们添加到env_free_list中。也要调用env_init_percpu来设置分段硬件为相互隔离权限等级0（内核）分段和权限等级3（用户）分段。
    env_setup_vm()
    为新环境分配一个页目录并初始化新环境的内核部分地址空间。
    region_alloc()
    为环境分配并映射物理内存
    load_icode()
    需要分析一个ELF二进制镜像，就像启动加载器做过的那样，并把它的内容加载到新环境的用户地址空间。
    env_create()
    用env_alloc分配一个环境并调用load_icode来加载一个ELF二进制文件到它里面。
    env_run()
    开始给定环境在用户模式下的运行
    完成这些函数后，新cprinf变量%e会很有用--它打印一个对应错误代码的描述。例如：
	r = -E_NO_MEM;
	panic("env_alloc: %e", r);
    输出为"env_alloc: out of memory"。  
  
env_init实现
```
void
env_init(void)
{
	// Set up envs array
	// LAB 3: Your code here.
    int i;
    // 反向遍历使列表中低位序列在前
    for (i = NENV-1; i >= 0; --i) {
        envs[i].env_id = 0;
    
        envs[i].env_link = env_free_list;
        env_free_list = &envs[i];
    }
	// Per-CPU part of the initialization
	env_init_percpu();
}
```
  
env_setup_vm()实现  
```
static int
env_setup_vm(struct Env *e)
{
	int i;
	struct PageInfo *p = NULL;

	// Allocate a page for the page directory
	if (!(p = page_alloc(ALLOC_ZERO)))
		return -E_NO_MEM;

	// LAB 3: Your code here.
	// 获得空闲页，页引用次数增加
     e->env_pgdir = (pde_t *)page2kva(p);
     p->pp_ref++;
	//UTOP下页目录映射为0
     for (i = 0; i < PDX(UTOP); i++)
         e->env_pgdir[i] = 0;  
    //UTOP上映射到内核页目录
     for (i = PDX(UTOP); i < NPDENTRIES; i++) {
         e->env_pgdir[i] = kern_pgdir[i];
     }

	// UVPT maps the env's own page table read-only.
	// Permissions: kernel R, user R
	e->env_pgdir[PDX(UVPT)] = PADDR(e->env_pgdir) | PTE_P | PTE_U;

	return 0;
}

```
region_alloc()实现  
```
static void
region_alloc(struct Env *e, void *va, size_t len)
{
	// 页对齐
	void* start = (void *)ROUNDDOWN((uint32_t)va, PGSIZE);
    void* end = (void *)ROUNDUP((uint32_t)va+len, PGSIZE);
    struct PageInfo *p = NULL;
    void* i;
    int r;
    for(i = start; i < end; i += PGSIZE){
		//分配页
        p = page_alloc(0);
        if(p == NULL)
           panic(" region alloc failed: allocation failed.\n");
        //插入页
        r = page_insert(e->env_pgdir, p, i, PTE_W | PTE_U);
        if(r != 0)
            panic("region alloc failed.\n");
    }
}
```
load_icode()实现  
```
static void
load_icode(struct Env *e, uint8_t *binary)
{
	//解析binary
	struct Elf* header = (struct Elf*)binary;

    if (header->e_magic != ELF_MAGIC) 
        panic("load_icode failed: The binary we load is not elf.\n");

    if (header->e_entry == 0)
        panic("load_icode failed: The elf file can't be excuterd.\n");

   e->env_tf.tf_eip = header->e_entry;
   //加载页目录
   lcr3(PADDR(e->env_pgdir));
   //加载ELF文件
   struct Proghdr *ph, *eph;
   ph = (struct Proghdr* )((uint8_t *)header + header->e_phoff);
   eph = ph + header->e_phnum;
    for(; ph < eph; ph++) {
        if(ph->p_type == ELF_PROG_LOAD) {
            if(ph->p_memsz - ph->p_filesz < 0) 
                panic("load icode failed : p_memsz < p_filesz.\n");

           region_alloc(e, (void *)ph->p_va, ph->p_memsz);
            memmove((void *)ph->p_va, binary + ph->p_offset, ph->p_filesz);
            memset((void *)(ph->p_va + ph->p_filesz), 0, ph->p_memsz - ph->p_filesz);
        }
     } 
   // 为环境的栈初始化一页
   region_alloc(e,(void *)(USTACKTOP-PGSIZE), PGSIZE);
}
```
env_create()实现  
```
void
env_create(uint8_t *binary, enum EnvType type)
{
	// LAB 3: Your code here.
    struct Env *e;
    int rc;
    if ((rc = env_alloc(&e, 0)) != 0)
          panic("env_create failed: env_alloc failed.\n");

     load_icode(e, binary);
     e->env_type = type;
}
```
env_run()实现  
```
void
env_run(struct Env *e)
{
	// LAB 3: Your code here.
	
    if(curenv != NULL && curenv->env_status == ENV_RUNNING)
        curenv->env_status = ENV_RUNNABLE;

    curenv = e;
    curenv->env_status = ENV_RUNNING;
    curenv->env_runs++;
    lcr3(PADDR(curenv->env_pgdir));

    env_pop_tf(&curenv->env_tf);

	//panic("env_run not yet implemented");
}
```

用户代码申请时的所有调用图如下：  

    start (kern/entry.S)
    i386_init (kern/init.c)
        cons_init
        mem_init
        env_init
        trap_init (此时尚未实现)
        env_create
        env_run
            env_pop_tf
  

完成后编译并在QEMU中有哪些它。一切顺利的化，系统会进入用户空间并执行hello二进制文件直达它申请了一个int指令系统调用。次数会出现问题，因为JOS还没有设置硬件来允许任何的用户空间到内核的切换。当CPU发现发现它还没被设置好处理这样的系统调用中断，它会产生一个通用保护异常，发现它不能处理这种情况，会产生第二个错误异常，发现它还不能这个异常，最后放弃，这就是所谓的三重错误。通常，CPU会重置然后系统会重启。虽然这对于古早的应用很重要，但对内核开发而言事痛苦的，所以QEMU作为替代会倒出寄存器和"Triple fault"信息。  
  
不久之后就来处理这个问题，但现在可以用debugger来检查我们是否进入了用户模式。`make qemu-nox-gdb`并设置一个GDB断点到env_pop_tf，在进入用户模式前的那个函数。单步调试这个函数；处理器在 iret指令后进入用户模式。可以看到第一个用户环境的可执行指令即lib/entry.s里start标签的cmpl指令。用`b *0x....`来设置hello里sysy_cputs()调用的`int $0x30`指令。这个int是在控制台显示一个字符的指令。在继续前确认能够执行到int。  
  
在/obj/user/hello.asm中找到指令  
`  800b0f:	cd 30                	int    $0x30`  
设置断点到0x800b0f并继续   
可以正确运行到断点处。   
  
### 处理中断和意外
在这个时候，第一个用户空间的int $0x30系统调用是个死胡同：一旦处理器进入用户模式，就没有任何方法回去了。现在需要开发基础意外和系统调用处理，好让内核可以从用户模式代码中恢复对处理器的控制。首先要做的是熟悉x86中断和意外机制。  
  
> + Exercise 3 阅读[80386程序员手册]（https://pdos.csail.mit.edu/6.828/2018/readings/i386/toc.htm）的第九章意外和中断  
  
本实验将大体上遵循Intel的术语如中断意外和其他。然而像意外，陷入，中断，错误这样的词在跨架构的操作系统上没有标准语义，在某种特定架构如x86上不会有太大差别，但此外的地方可能会有些许不同。  
  
### 保护控制切换基础
意外和中断都是“被保护的控制切换”，使得处理器从用户切换到内核模式（CPL=0)而不用给用户模式与任何内核功能和其他环境交互的机会。在Intel的术语中，一个中断是一个保护控制切换，通常由处理器外部的异步事件引起，如外部设备的I/O活动通知。一个意外，与此相对的，被当前运行代码同步引起的保护控制切换，例如除0和无效内存访问。  
  
为了确保这些保护控制切换确实被保护了，处理器的中断/意外机制被设计出来，使得当前运行的代码在中断或意外发生时不用随便的选择内核从哪里与如何进入。作为替代，处理器确保了内核只可以在仔细地控制的条件下进入。在x86上，两个机制共同运作来提供保护：  
  
> + 1. 中断描述符表 
       处理器确保中断和意外只能引起内核从它决定几个特定的仔细定义的入口进入，而不是被中断或意外发生时从运行代码决定。 
       x86允许多大256给不同的中断和意外内核入口，每个有一个不同的中断向量。一个向量是0-255之间的数字。一个中断向量被中断的来源决定：不同的设备，错误情况；对内核的应用要求产生不同向量的中断。CPU使用向量作为处理器中断描述符表的序列号，就像GDT一样，被内核设置在内核私有的内存中。从正确的入口进入这个表后处理器加载：
       加载到指令寄存器（EIP)的值，它指向处理这种意外类型的内核代码。
       加载到代码段寄存器（CS)的值，包括意外处理器运行的权限等级的bit位。（在JOS里，所有的意外都在内核模式里处理，权限等级0。）
> + 2. 任务状态分段
       处理器需要一个地方来存储中断或意外发生前的处理器状态，例如在处理器申请意外处理前原本EIP和CS的值，好让意外处理其之后能回复旧状态并回到中断代码离开的地方。但这个为旧处理器状态保存的区域一定要对用户模式代码保护；不然出错和恶意的用户代码会使内核遭受风险。  
       因此，当x86处理器因为中断或陷入引起用户到内核权限等级切换时，它也会切换到内核内存中一个栈。名为任务状态段(TSS)的结构详述了这个分段选择符和栈所在的地址。处理器压入(在这个新栈中）SS，ESP，EFLAGS，CS，EIP和一个可选的错误码。然后处理器会从中断描述符中加载CS和EIP，并设置ESP和SS指向新栈。 
       尽管TSS很大有能服务多种目的的潜力，JOS只把它用来定义从用户到内核模式切换时处理器应该切换到的内核栈。因为JOS里内核模式的权限等级在xv6上是0，处理器使用TSS的ESP0和SS0字段在进入内核模式时定义内核栈。JOS不使用其他的TSS字段。
  
  
### 意外和中断的类型
所有x86处理器可以内部产生的同步意外使用中断向量0-31,即IDT0-31项记录。例如一个页错误导致一个向量14的意外。大于31的中断向量只被程序中断使用，如一个int指令或者有外部设备引起异步硬件中断。  
  
在这一块我们将拓展JOS来处理内部产生的0-31号向量意外。在下一节使JOS处理软件中断向量48（0x30)，JOS(相当随意地）用来当作它地系统调用中断向量。Lab4中会拓展JOS来处理外部产生地硬件中断例如时钟中断。  
  
### 一个例子
把这些片段放在一起并通过一个例子来追踪。加上处理器正在用户环境里执行代码，遭遇了了除法指令尝试除零。    
 1.处理器切换到TSS字段SS0和ESP0定义的栈，即JOS将分别保存GD_KD和KSTACKTOP的地方。    
 2.处理器压入意外参数到内核栈，从地址KSTACKTOP开始：  
                     +--------------------+ KSTACKTOP             
                     | 0x00000 | old SS   |     " - 4
                     |      old ESP       |     " - 8
                     |     old EFLAGS     |     " - 12
                     | 0x00000 | old CS   |     " - 16
                     |      old EIP       |     " - 20 <---- ESP 
                     +--------------------+             
 3.因为在处理一个除法错误，其中断向量在x86上为0，处理器读取IDT记录0并设置CS:EIP来指向这个记录描述的处理函数入口。   
 4.处理函数活动控制权并处理意外，例如中断用户环境。  
  
对于特定类型的x86意外，除了上面“标准”的五个词节外，处理器还压入了一个包含错误码的词节到栈里。页错误意外，编号14，使一个重要的例子。检查80386操作手册来决定处理器压入的错误代表了什么意外数字。当处理器压入一个错误码，从用户模式进入在意外处理开始时栈会像下面的样子：  
                     +--------------------+ KSTACKTOP             
                     | 0x00000 | old SS   |     " - 4
                     |      old ESP       |     " - 8
                     |     old EFLAGS     |     " - 12
                     | 0x00000 | old CS   |     " - 16
                     |      old EIP       |     " - 20
                     |     error code     |     " - 24 <---- ESP
                     +--------------------+             
  
### 嵌套的意外和中断
处理器可以从内核与用户模式中接受意外和中断。然而，只有在从用户模式进入内核时，x86处理器才在压入它的旧寄存器状态和通过IDT申请恰当的意外处理前，自动地切换栈。如果处理器已经在内核模式里（CS寄存器的低二位已经使0），当中断或意外发生时，CPU至少在相同的内核栈里压入更多的值。通过这种方法，内核可以优雅地处理内核自己引起的嵌套意外。这个能力在实现保护机制时是非常重要的工具，可以在之后系统调用的分节认识到。  
  
如果处理器已经在内核模式并接受了一个嵌套意外，因为它不需要切换栈，它也不用存储旧的SS或ESP寄存器。对于不雅如错误码的因为类型，在进入意外处理器时内核栈会像下面的结构：  
                     +--------------------+ <---- old ESP
                     |     old EFLAGS     |     " - 4
                     | 0x00000 | old CS   |     " - 8
                     |      old EIP       |     " - 12
                     +--------------------+  
  
对于压入错误码的意外类型，处理器紧接在Old EIP后压入错误码。  
  
对于处理器的嵌套意外能力有一个警告：如果处理器已经在内核模式里时接受了一个意外，而因为某些原因（如缺少栈空间）不能压入它的旧状态到内核栈里，那么处理器就无法恢复到原状态，所有它就会简单的重置它自己。不用说的是，内核应该被设计成避免这种状况。  
  
### 设置IDT
  
现在有了在JOS里设置IDT和处理意外的基础信息。开始设置IDT并处理0-31号中断向量。之后的实验中将处理系统调用并增加32-47号（设备IRQ)中断。  
  
头文件inc/trap.h和kern/trap.h包含了中断意外相关的重要定义。文件kern/trap.h包含了内核严格私有的定义，而inc/trap.h包含了用户等级程序和库有用的定义。    
  
注意：一些0-31范围的意外被Intel定义为保留。因为它们永远不会被处理器产生，所有怎么处理它们是无关紧要的。最干净地完成这里。    
  
总统流程图如下：  

      IDT                   trapentry.S         trap.c
   
+----------------+                        
|   &handler1    |---------> handler1:          trap (struct Trapframe *tf)
|                |             // do stuff      {
|                |             call trap          // handle the exception/interrupt
|                |             // ...           }
+----------------+
|   &handler2    |--------> handler2:
|                |            // do stuff
|                |            call trap
|                |            // ...
+----------------+
       .
       .
       .
+----------------+
|   &handlerX    |--------> handlerX:
|                |             // do stuff
|                |             call trap
|                |             // ...
+----------------+
  
每个意外或者中断应该有它自己的处理器，在trapentry.S里，而tarp_init()应该用这些处理器的地址初始化IDT。每个处理器应该构建一个struct Trapframe(在inc/trap.h)到栈里并用Trapframe指针调用trap()(在trap.c中)。trap()之后处理这个意外/中断或者分发到一个特定的处理函数。  
  
> + Exercise 4 
    编辑trapentry.S和trap.c并实现上面描述的功能。trapentry.S里的宏TRAPHANDLER 和TRAPHANDLER_NOEC 会很有用，inc/trap.h中的T_*类也是。需要为每个inc/trap.h中定义的陷入在trapentry.S里添加一个记录，还需要提供TRAPHANDLER宏指向的_alltraps。另外修改trap_init()来初始化idt来指向trapentry.S里的每个入口；这里SETGATE宏会很有用。  
    _alltaps完成下列工作:
    1 压入值使得栈看起来像一个Trapframe结构
    2 加载GD_KD到%ds和%es
    3 用pushl %esp来传递一个Trapframe指针参数给trap()
    4 调用 trap
    思考pushal的用法，它很适合struct Trapframe的布局。
    用user目录下在系统调用前引起意外的测试程序（如user/divzero）来测试陷入处理代码。
  
在trapentry.S中为不同的陷入设置入口  
```
/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
TRAPHANDLER_NOEC(divide_entry, T_DIVIDE);
TRAPHANDLER_NOEC(debug_entry, T_DEBUG);
TRAPHANDLER_NOEC(nmi_entry, T_NMI);
TRAPHANDLER_NOEC(brkpt_entry, T_BRKPT);
TRAPHANDLER_NOEC(oflow_entry, T_OFLOW);
TRAPHANDLER_NOEC(bound_entry, T_BOUND);
TRAPHANDLER_NOEC(illop_entry, T_ILLOP);
TRAPHANDLER_NOEC(device_entry, T_DEVICE);
TRAPHANDLER(dblflt_entry, T_DBLFLT);
TRAPHANDLER(tss_entry, T_TSS);
TRAPHANDLER(segnp_entry, T_SEGNP);
TRAPHANDLER(stack_entry, T_STACK);
TRAPHANDLER(gpflt_entry, T_GPFLT);
TRAPHANDLER(pgflt_entry, T_PGFLT);
TRAPHANDLER_NOEC(fperr_entry, T_FPERR);
TRAPHANDLER(align_entry, T_ALIGN);
TRAPHANDLER_NOEC(mchk_entry, T_MCHK);
TRAPHANDLER_NOEC(simderr_entry, T_SIMDERR);
TRAPHANDLER_NOEC(syscall_entry, T_SYSCALL);
```
_alltraps的实现  
```
_alltraps:
        pushl %ds
        pushl %es
        pushal

        movl $GD_KD, %eax
        movl %eax, %ds
        movl %eax, %es

        push %esp
        call trap
```
trap_init(void)初始化  
```
void
trap_init(void)
{
	extern struct Segdesc gdt[];
    extern void divide_entry();
    extern void debug_entry();
    extern void nmi_entry();
    extern void brkpt_entry();
    extern void oflow_entry();
    extern void bound_entry();
    extern void illop_entry();
    extern void device_entry();
    extern void dblflt_entry();
	extern void tss_entry();
    extern void segnp_entry();
    extern void stack_entry();
    extern void gpflt_entry();
    extern void pgflt_entry();
    extern void fperr_entry();
    extern void align_entry();
    extern void mchk_entry();
    extern void simderr_entry();
    extern void syscall_entry();

	// LAB 3: Your code here.
        SETGATE(idt[T_DIVIDE], 0, GD_KT, divide_entry, 0);
        SETGATE(idt[T_DEBUG], 0, GD_KT, debug_entry, 0);
        SETGATE(idt[T_NMI], 0, GD_KT, nmi_entry, 0);
        SETGATE(idt[T_BRKPT], 0, GD_KT, brkpt_entry, 3);
        SETGATE(idt[T_OFLOW], 0, GD_KT, oflow_entry, 0);
        SETGATE(idt[T_BOUND], 0, GD_KT, bound_entry, 0);
        SETGATE(idt[T_ILLOP], 0, GD_KT, illop_entry, 0);
        SETGATE(idt[T_DEVICE], 0, GD_KT, device_entry, 0);
        SETGATE(idt[T_DBLFLT], 0, GD_KT, dblflt_entry, 0);
        SETGATE(idt[T_TSS], 0, GD_KT, tss_entry, 0);
        SETGATE(idt[T_SEGNP], 0, GD_KT, segnp_entry, 0);
        SETGATE(idt[T_STACK], 0, GD_KT, stack_entry, 0);
        SETGATE(idt[T_GPFLT], 0, GD_KT, gpflt_entry, 0);
        SETGATE(idt[T_PGFLT], 0, GD_KT, pgflt_entry, 0);
        SETGATE(idt[T_FPERR], 0, GD_KT, fperr_entry, 0);
        SETGATE(idt[T_ALIGN], 0, GD_KT, align_entry, 0);
        SETGATE(idt[T_MCHK], 0, GD_KT, mchk_entry, 0);
        SETGATE(idt[T_SIMDERR], 0, GD_KT, simderr_entry, 0);
        SETGATE(idt[T_SYSCALL], 0, GD_KT, syscall_entry, 3);
	// Per-CPU setup 
	trap_init_percpu();
}
```
至此，Part a score 30/30  
  
> + challenge
    现在代码可能有大量相似的地方，具体来说在表TRAPHANDLER和trap.c中的初始化。整理这些代码，改变trapentry.S的宏来自动生成一个表让trap.c来使用。注意使用伪指令.text和.data来在汇编器里切换底层代码和数据。
  
宏TRAPHANDLER修改（宏中注意末尾空格）：  
```
#define TRAPHANDLER(name, num, ec, user)				\
.text;									\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	.if ec==0;							\
		pushl $0;						\
	.endif;								\
	pushl $(num);							\
	jmp _alltraps;							\
.data;									\
	.long  name, num, user	
```
  
定义并填充全局数组：  
```
.data
	.globl  entry_data
	entry_data:
.text
TRAPHANDLER(divide_entry, T_DIVIDE, 0, 0);
TRAPHANDLER(debug_entry, T_DEBUG, 0, 0);
TRAPHANDLER(nmi_entry, T_NMI, 0, 0);
TRAPHANDLER(brkpt_entry, T_BRKPT, 0, 1);
TRAPHANDLER(oflow_entry, T_OFLOW, 0, 0);
TRAPHANDLER(bound_entry, T_BOUND, 0, 0);
TRAPHANDLER(illop_entry, T_ILLOP, 0, 0);
TRAPHANDLER(device_entry, T_DEVICE, 0, 0);	
TRAPHANDLER(dblflt_entry, T_DBLFLT, 1, 0);
TRAPHANDLER(tts_entry, T_TSS, 1, 0);
TRAPHANDLER(segnp_entry, T_SEGNP, 1, 0);
TRAPHANDLER(stack_entry, T_STACK, 1, 0);
TRAPHANDLER(gpflt_entry, T_GPFLT, 1, 0);
TRAPHANDLER(pgflt_entry, T_PGFLT, 1, 0);
TRAPHANDLER(fperr_entry, T_FPERR, 0, 0);
TRAPHANDLER(align_entry, T_ALIGN, 1, 0);
TRAPHANDLER(mchk_entry, T_MCHK, 0, 0);
TRAPHANDLER(simderr_entry, T_SIMDERR, 0, 0);
TRAPHANDLER(syscall_entry, T_SYSCALL, 0, 1);
.data
	.long 0, 0, 0   // interupt end identify
```  
利用全局数组进行初始化
```
void
trap_init(void)
{
        extern struct Segdesc gdt[];
        extern long entry_data[][3];
        int i;

        for (i = 0; entry_data[i][0] != 0; i++ )
                SETGATE(idt[entry_data[i][1]], 0, GD_KT, entry_data[i][0], entry_data[i][2]*3);

        trap_init_percpu();
}
```
  
  
> + Questions
    1.每个意外/中断都有独立的处理函数的目的是什么？（例如，如果所有意外/中断分到同一个处理函数当前实现的那个功能就失效了？
    2.如果是否尝试使得uer/softint程序正确运行？打分脚本期待它产生一个通用保护错误（trap13)，但softint的代码认为是int $14.为何这会产生中断向量13？如果内核实际允许softint的int $14指令申请内核页错误处理器会发生什么？（中断向量14是什么？）
  
1.这样的隔离可以使每个意外/中断只申请到处理它所需要的部分，给予尽量少的内核资源；如果统一一个函数来处理的话就丧失了意外/中断机制的保护性。  
  
2.中断向量14为页错误，其权限等级要求为零，程序在用户空间请求时会触发通用保护错误。  
  
## Part b 页错误，断点意外和系统调用
现在内核有了基础的意外处理能力，优化它使得能够提供基于意外处理的重要的操作系统原语。  
  
### 处理页错误  
  
页错误意外，中断向量为14（T_PGFLT),是一个尤其重要的功能，在本次和之后的实验会频繁实践。当处理器接受了一个页错误，它会存储引起这个错误的线性（虚拟）地址到一个特定的处理器控制寄存器CR2中。在trap.c中已有这个特别函数的开头，page_fault_handler()，来处理页错误意外。  
  
> + Exercise 5
    修改trap_dispatch()来分发页错误到page_fault_handler()。现在应该可以使得make grade成功通过faultread,faultreadkernel,faultwritekernel测试。记住可以用make run-x或者make run-x-nox来启动JOS到一个特定的用户程序。例如，make run-hello-nox来运行hello用户程序。
  
trap_dispatch（）实现为  
```
static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.
    if (tf->tf_trapno == T_PGFLT) {
        page_fault_handler(tf);
        return;
    }
	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}
}
```  
测试通过  

下面会进一步优化内核页错误处理，在完成系统调用之后。  
  
### 断点意外
  
断点意外，中断向量3（T_BRKPT),通常被用来让debugger插入断点到程序代码里，以特别的1字节int3软件中断指令替代想换程序指令的方式。在JOS里会稍微滥用这个意外，把它转变成一个原始的伪系统调用，使得所有用户环境可以用它来申请JOS内核监控器。如果把JOS内核监控器当作原始的debugger的话，这个用法实际上很正确。用户环境实现的panic()（在lib/panic中）在显示了它的错误信息后就会执行一个int3，这就是一个例子。  
  
> + Exercise 6
    修改trapdispatch()来让断点意外申请内核监控器。让make grade通过breakpoint测试。  
  
trapdispatch()新实现,改用switch判断  
  
```
static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.
	switch (tf->tf_trapno)
	{
    case T_PGFLT:
        page_fault_handler(tf);
        break;
    case T_BRKPT:
        monitor(tf);
        break;
	default:
	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	     }
	}	 
}
```
测试通过  
  
> + Question
    3.断点测试情况下会基于在IDT里初始化断点记录的方法来产生一个断点意外或是通用保护错误。为什么？如果为了获得像上面陈述的一样的断点意外要怎么设置？是什么错误的设置导致它触发了通用保护错误？  
    4.你认为这些机制的关键点是什么，尤其是从user/sofint测试程序做的来看？  
  
3.在IDT中注册断点意外时，如果参数DPL为3，则可以获得断点意外；而设置为0时，是在从用户环境里要求内核权限等级的调用，故会引起通用保护错误。  
  
4.关键在于操作权限的隔离，可以有效避免用户程序错误或恶意地威胁内核。  
  
### 系统调用
  
用户进程通过申请系统调用来要求内核为它们做事。当用户进程申请了一个系统调用，处理器进入内核模式，处理器与内核合作存储用户进程地状态，内核执行正确地代码来实行系统调用，然后恢复到用户进程。不同系统间用户进程获得内核注意和陈述他想要执行哪个调用的具体实现是不同的。  
  
在JOS内核中，使用int指令来引起一个处理器中断。实际上，使用int $0x30来作为系统调用中断。T_SYSCALL已经被定义为48(0x30)以供使用。需要设置中断描述符来允许用户进程来引起这个中断。注意中断0x30不能被硬件产生，所以允许用户代码产生它不会产生歧义。  
  
应用会传递系统调用数字和寄存器中的系统调用参数。这个方法下，内核不需要从用户环境栈里抓取指令流。系统调用数字放在%eax中，而参数（至多五个）分别放在%edx,%ecx,%ebx,%edi和%esi中。内核传递返回值到%eax里。申请系统调用的汇编代码已经完成，在lib/syscall.c的syscall()里。通读并理解发生了什么。  
  
> + Exercise 7
    添加一个中断向量T_SYSCALL的处理器到内核中。需要编辑kern/trapentry.S和kern/trap.c的trap_init()。也需要改变trap_dispatch通过用正确的参数调用syscall()（在kern/syscall.c里）来处理系统调用中断，然后安排返回值到%eax里回传到用户进程。最终，需要实现kern/syscall.c里的syscall()。确认如果系统调用数字无效时syscall()返回-e_INVAL。阅读并理解lib/syscall.c(尤其是行内汇编方法）来证实你对系统调用交互的理解。处理所有inc/syscall.h里列出的系统调用，通过为每个调用申请对应的内核函数。
    在内核里运行user/hello程序。它应该会打印"hello world"到控制台，之后引起一个用户模式的页错误。如果不是这样的话意味着系统调用处理器还没有实现好。最终需要通过make grade的testbss测试。  
  
kern/trapentry.S中注册系统调用中断   
   
`TRAPHANDLER(syscall_entry, T_SYSCALL, 0, 1);`  
  
trap_init（）中由之前的实现自动初始化  
  
trap_dispatch()的switch中添加系统调用case  
  
```
	int32_t ret_code;
    case (T_SYSCALL):
    //传入tf寄存器的值作为参数
        ret_code = syscall(
                    tf->tf_regs.reg_eax,
                    tf->tf_regs.reg_edx,
                    tf->tf_regs.reg_ecx,
                    tf->tf_regs.reg_ebx,
                    tf->tf_regs.reg_edi,
                    tf->tf_regs.reg_esi);
    //返回值放入%eax中回传给用户环境
        tf->tf_regs.reg_eax = ret_code;
        break;
```
inc/syscall.h中枚举为  
```
enum {
	SYS_cputs = 0,
	SYS_cgetc,
	SYS_getenvid,
	SYS_env_destroy,
	NSYSCALLS
};
```
根据要求完成kern/syscall.c中syscall()  
```
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.

	//panic("syscall not implemented");
    switch (syscallno) {
        case (SYS_cputs):
            sys_cputs((const char *)a1, a2);
            return 0;
        case (SYS_cgetc):
            return sys_cgetc();
        case (SYS_getenvid):
            return sys_getenvid();
        case (SYS_env_destroy):
            return sys_env_destroy(a1);
        default:
            return -E_INVAL;
    }
}
```
测试通过  
  
### 用户模式启动
  
一个用户程序从lib/entry.S的最顶部开始运行。在一些设置后，这个代码调用了lib/libmain.c中的libmain()。修改libmian()来初始化全局指针thisenv来执行这个环境在envs[]数组中的struct Env。（注意lib/entry.S已经定义了envs指向Part a中设置的UENVS映射。）提示：检查inc/env.h并使用sys_getenvid。  
  
libmain()之后调用umain，在hello程序的情况下，放在user/hello.c里。注意在打印"hello,world"后，它尝试访问this->env_id。这就是为什么它会更早出错。现在正确初始化thisenv后，它就不会报错了。如果仍然报错，可能是UENVS区域没有被映射为用户可读（在Part a的部分；这是第一次实际使用UENVS区域）。  
  
> + Exercise 8
    添加要求的代码到用户库里，然后启动内核。之后应该能看到uer/hello打印“hello，world”然后打印“i am environment 00001000”。user/hello之后打算通过调用sys_env_destroy（在lib/libmain.c和lib/exit.c中）来"退出"。因为内核现在只支持一个用户环境，它会报告它已经销毁了唯一的环境并掉入内核监控器。现在make grade应该能通过hello测试。  
  
libmain中初始化thisenv  
```
void
libmain(int argc, char **argv)
{
	// set thisenv to point at our Env structure in envs[].
	// LAB 3: Your code here.
	// 获取环境id并转换为envs数组序列
	thisenv = envs + ENVX(sys_getenvid());

	// save the name of the program so that panic() can use it
	if (argc > 0)
		binaryname = argv[0];

	// call user main routine
	umain(argc, argv);

	// exit gracefully
	exit();
}
```
测试通过  
  
### 页错误和内存保护
  
内存保护是一个操作系统的关键功能，确保一个程序的bug不会破坏其他程序或者操作系统自身。  
  
操作系统通常依赖硬件支持来实现内存保护。操作系统保持通知硬件耐心虚拟地址是有效的和哪些无效的。当一个程序尝试访问一个无效的地址或者一个它没有许可的，处理器终止程序在引起错误的指令执行时，然后和请求操作的信息一起陷入内核态。如果这个错误是可修复的，内核可以修复它并让程序继续运行。如果错误不可修复，那么程序就无法继续，因为它永远无法通过引起错误的指令。  
  
作为一个可修复错误的例子，思考一个自动扩展的栈。在很多系统里内核初始化分配了一个栈单页，然后如果程序错误访问了更低位的页，内核会自动分配这些页并让程序继续。这样做内核只要分配程序所需要的栈内存就足够了，但程序可以在以为自己有足够大的栈的幻象中工作。  
  
系统调用展现了一个有趣的内存保护问题。大多系统调用交互让用户程序传递指针给内核。这些指针指向用户的读写缓冲区。内核之后在实行系统调用时解引用这些指针。这里有两个问题：  
  
1.内核的一个页错误比用户程序的页错误潜在地严重地多。如果内核在操作它自己地数据结构时产生了页错误，那就是一个内核bug，错误处理器应该向内核报错（也是整个系统）。但当内核正在解引用用户程序传递给它地指针，它需要一个方法来记住所有地这些解引用造成的页错误实际上是用户程序带来的。  
  
2.内核通常比用户程序有更多的内存权限。用户程序可能会向系统调用传递一个指向它不能访问但内核可以读写的内存指针。内核一定要小心不被这样的指针解引用欺骗，因为这可能会泄露私有信息或者破坏内核的完整性。  
  
这两个原因使得内核在处理用户程序指针时必须极端小心。  
  
现在用一个简单的检查所有用户空间传递给内核的指针的机制来解决这两个问题。当一个程序传递给内核一个指针，内核会检查这个地址是否在用户部分的地址空间，和页表是否允许这个内存操作。  
  
因此内核永远不会因为解引用一个用户提供的指针而收到页错误。如果内核产生了页错误，它应该报错并终止。  
  
> + Exercise 9 
    改变kern/trap.c使得内核模式页错误方式时报错。
    提示：为了确认一个页错误发生在用户模式还是内核模式，检查tf_cs的低位。 
    阅读kern/pmap.c里的user_mem_assert函数并在同一个文件里实现user_mem_check。
    改变kern/syscall.c来清楚地检查系统调用的参数。  
    启动内核，允许user/buggyhello。这个环境应该被销毁，并且内核不会报错，输出如下：
    [00001000] user_mem_check assertion failure for va 00000001
	[00001000] free env 00001000
	Destroyed the only environment - nothing more to do!
    最后，改变kern/kdebug.c里的debuginfo_eip来调用user_mem_check检查usd,stabs和stabstr。如果现在运行user/breakpoint，就应该能够从内核监控器里运行backtrace指令并看到反向追踪穿越到lib/libmain.c中，在内核报出一个页错误之前。什么引起了这个页错误？不必修复，但要理解发生了什么。
  
在page_fault_handler里实现模式检查  
```
void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.

	// LAB 3: Your code here.
    if(tf->tf_cs && 0x01 == 0) {
        panic("page_fault in kernel mode, fault address %d\n", fault_va);
    }	

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}

```
  
user_mem_assert内部调用了user_mem_check  
user_mem_check检查页权限的实现
```  
int
user_mem_check(struct Env *env, const void *va, size_t len, int perm)
{
	// LAB 3: Your code here.
    char * end = NULL;
    char * start = NULL;
    start = ROUNDDOWN((char *)va, PGSIZE); 
    end = ROUNDUP((char *)(va + len), PGSIZE);
    pte_t *cur = NULL;
    for(; start < end; start += PGSIZE) {
        cur = pgdir_walk(env->env_pgdir, (void *)start, 0);
        if((int)start > ULIM || cur == NULL || ((uint32_t)(*cur) & perm) != perm) {
              if(start == ROUNDDOWN((char *)va, PGSIZE)) {
                    user_mem_check_addr = (uintptr_t)va;
              }
              else {
                      user_mem_check_addr = (uintptr_t)start;
              }
              return -E_FAULT;
        }
    }
	return 0;
}
```
  
在sys_cputs中检查访问权限  
```
static void
sys_cputs(const char *s, size_t len)
{
    // Check that the user has permission to read memory [s, s+len).
    // Destroy the environment if not:.

    user_mem_assert(curenv, s, len, 0);
    // Print the string supplied by the user.
    cprintf("%.*s", len, s);
}
```  
在kern/kdebug.c里检查地址有效和访问权限  
```
int
debuginfo_eip(uintptr_t addr, struct Eipdebuginfo *info)
{
	const struct Stab *stabs, *stab_end;
	const char *stabstr, *stabstr_end;
	int lfile, rfile, lfun, rfun, lline, rline;

	// Initialize *info
	info->eip_file = "<unknown>";
	info->eip_line = 0;
	info->eip_fn_name = "<unknown>";
	info->eip_fn_namelen = 9;
	info->eip_fn_addr = addr;
	info->eip_fn_narg = 0;

	// Find the relevant set of stabs
	if (addr >= ULIM) {
		stabs = __STAB_BEGIN__;
		stab_end = __STAB_END__;
		stabstr = __STABSTR_BEGIN__;
		stabstr_end = __STABSTR_END__;
	} else {
		// The user-application linker script, user/user.ld,
		// puts information about the application's stabs (equivalent
		// to __STAB_BEGIN__, __STAB_END__, __STABSTR_BEGIN__, and
		// __STABSTR_END__) in a structure located at virtual address
		// USTABDATA.
		const struct UserStabData *usd = (const struct UserStabData *) USTABDATA;

		// Make sure this memory is valid.
		// Return -1 if it is not.  Hint: Call user_mem_check.
		// LAB 3: Your code here.
    if (user_mem_check(curenv, usd, sizeof(*usd),PTE_U) <0 )
        return -1;
		stabs = usd->stabs;
		stab_end = usd->stab_end;
		stabstr = usd->stabstr;
		stabstr_end = usd->stabstr_end;

		// Make sure the STABS and string table memory is valid.
		// LAB 3: Your code here.
    if (user_mem_check(curenv, stabs, stab_end-stabs,PTE_U) <0 || user_mem_check(curenv, stabstr, stabstr_end-stabstr,PTE_U) <0)
            return -1;		
	}
```
  
至此，所有测试通过，score: 80/80
  
注意刚刚实现的机制同样对恶意用户程序有效（例如user/evilhello)。  
  
> + Exercise 10
    启动内核，运行user/evilhello。环境应该被销毁，而内不会报错，输出如下：
	[00000000] new env 00001000
	...
	[00001000] user_mem_check assertion failure for va f010000c
	[00001000] free env 00001000    
