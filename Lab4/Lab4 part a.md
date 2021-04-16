#Lab 4 抢占式多任务
  
##介绍
这个实验中将实现几个同时活动的用户模式环境的抢占式多任务机制。  
  
Part a 添加JOS的多处理器支持，实现round-robin架构并添加基础的环境管理系统调用（调用来创建和销毁环境，并分配映射内存）。  
  
Part b 实现类Unix的fork(），允许用户模式环境来创建自己的拷贝。  
  
Part c 添加进程间通信机制，允许不同用户模式环境显式地互相通信和同步。同样要添加硬件时钟中断和抢占的支持。  
  
Lab 4 添加的一系列新文件，其中有一些应该在开始前浏览：  
```
kern/cpu.h 			内核私有的多处理器支持定义
kern/mpconfig.c 	读取多处理器设置的代码
kern/lapic.c 		驱动每个处理器局部APIC单元的内核码
kern/mpentry.S 		非启动CPU的汇编语言进入代码
kern/spinlock.h 	内核私有的自旋锁定义，包括大内核锁
kern/spinlock.c 	实现内核自旋锁的代码
kern/sched.c 		等待实现的调度代码框架
```  
  
##Part a 多处理器支持和协同任务
  
本实验的第一部分需要拓展JOS来运行一个多处理器系统，然后实现一些新的JOS系统调用来允许用户等级环境来创建额外的新环境。同样要实现协作的rund-robin调度，允许内核在当前环境自主放弃CPU(或者退出）时从一个环境切换到另一个。之后的part c里将实现抢占式调度，允许内核在一定时间后从一个不协作的环境手中重获CPU控制。  
  
###多处理器支持  
  
使JOS支持“对称多处理”，一个所有CPU有着同等系统资源（如内存和I/O总线）访问权限的多处理模型。尽管在SMP中所有CPU功能上完全一样，在启动过程中它们被分为两类：启动引导处理器（BSP)负责初始化环境管理系统和操作系统；而应用处理器（AP）只在操作系统启动运行后被BSP启用。由硬件和BIOS决定哪个处理器为BSP。到目前为止，所有代码都运行在BSP上。  
  
在一个SMP系统里，每个CPU都有一个配套的局部APIC(LAPIC)单元。LAPIC单元负责分发系统的中断。LAPIC也提供它链接的CPU一个唯一标识。本实验中将使用LAPIC单元（在kern/lapic.c)的下列基础功能： 
  
> + 读取LAPIC标识（LAPIC ID)来指示当前代码运行在哪个CPU上（见cpunum())。  
> + 从BSP发送STARTUP跨处理器中断（IPI) 到AP来启动其他CPU。  
> + 在part c，编程LAPIC的内构计时器来触发时钟中断以支持抢占式多任务（见apic_init())。
  
一个处理器用内存映射I/O(MMIO)来访问它的LAPIC。在MMIO中，物理内存的一部分被硬件嵌入到一些I/O设备的寄存器，所以通常使用的访问内存的加载/存储指令不能用来访问设备驱动寄存器。之前已经了解了物理地址0xA0000处的一个IO空洞（使用这个来写入到VGA显示缓冲区）。LAPIC处于物理地址0xFE000000（距离4GB只有32MB处的一个空洞，这对用通常在KERNBASE的直接映射来访问而言太高了。JOS虚拟内存映射在MMIOBASE保留了一个4MB的空缺来映射这样的设备。后面的实验hi更多地介绍MMIO区域，现在写一个简单的函数来从这个区域分配空间并映射设备内存给它。  
  
> + Exercsie 1
    实现kern/pamp.c的mio_map_region。在kern/lapic.c的lapic_init里看看它是怎么使用的。在mmio_map_region的测试运行前，还要完成下个练习。  
  
lapic_init()中使用mmio_map_region的语句为  
`lapic = mmio_map_region(lapicaddr, 4096);`  
lapicaddr为LAPIC的4KMMIO区域地址。  
  
映射前需要对地址和大小进行页边界对齐  
由此反向推导实现mmio_map_region    
```
void *
mmio_map_region(physaddr_t pa, size_t size)
{
	static uintptr_t base = MMIOBASE;
	// Your code here:
	//panic("mmio_map_region not implemented");
	size = ROUNDUP(size,PGSIZE);
	pa = ROUNDDOWN(pa,PGSIZE);
	if (size + base >= MMIOLIM)
		panic("mmio_map_region overflow");
	boot_map_region(kern_pgdir,base,size,pa,PTE_PCD|PTE_PWT|PTE_W);
	uintptr_t ret = base;
	base = base + size;
	return (void*)ret;
}
```
  
###应用处理器启动
  
在启动AP之前，BSP首先应该手机多处理器系统的信息，例如CPU总数。它们的APIC ID和LAPIC单元的MMIO地址。kern/mpconfig里的mp_init()函数通过读取放在BIOS的内存区域的MP设置表来找到这些信息。  
  
boot_aps（）函数（kern/init.c)驱动了AP的启动过程。AP开始在实模式，就像启动加载器在boot/boot.S里一样，所以boot_aps（）拷贝了AP的入口码（kern/mpentry.S）到一个实模式可寻址的内存区域。不像启动加载器的是，我们可以控制AP从哪里开始执行代码；拷贝进入码到0x7000(MPENTRY_PADDR)，但其他低于640KB没用过的页分配过的物理地址也可以。  
  
在此之后，boot_aps()一个一个地激活APs，以发送STARTUP IPI到AP对应的LAPIC单元的方式，随之发送的还有AP应该开始运行它的的入口代码（MPENTRY_PAADR)的初始化CS:IP地址。kern.mpentry.S的入口码很像boot/boot.S里的。在一些简单的设置后，它把AP放入保护模式，启动分页功能然后调用C语言启动路径mp_main()（也在kern/init.c)。在继续唤醒下一个前，boor_aps()等待AP发出一个CPU_STARTED标志位到它的struct CpuInfo的cpu_status字段。  
  
> + Exercise 2
    阅读kenr/init.c的boot_aps()和mp_main()，然后是kern/mpentry.S的汇编代码。确保理解了AP启动阶段的控制流转换。然后修改kern/pmap.c中的page_init()实现来避免添加页到MPENTRY_PADDR到空闲列表中，好让AP启动代码可以安全的拷贝运行到那个物理地址上。代码需要通过更新过的	check_page_free_list()测试。
  
在page_init()的for循环中添加MPENTRY_PADDR分配情况：  
```
		if (i == 0 || i == MPENTRY_PADDR/PGSIZE) {
			pages[i].pp_ref = 1;
			pages[i].pp_link = NULL;
			continue;
		}
```
check_page_free_list() 测试通过。


> + Question
    1.比较kern.mpentry.S和boot/boot.S。记在脑海里，kern/mperntry.S被编译和链接在KERNBASE之上运行，就像内核里的其他东西一样，那么宏MPBOOTPHYS的目的是什么？为什么它在kern/mpentry.S里很重要而在boot/boot.S里不是？换句话说，如果它在kern/mpentry.S里被删除了什么会出错？
    提示回想Lab1里讨论的链接地址和加载地址的区别。
  
MPBOOTPHYS宏用于转换每个CPU各自符号的绝对地址。  
AP启动时未进入保护模式不能访问高位空间，不能通过链接地址访问加载地址，而在MP开启了分页功能的情况下也不能直接访问物理地址，所以需要宏由线性地址来算出物理地址，删除会导致AP无法找到符号的加载地址；而boot.S中未启动分页机制时链接和加载地址一致，可以直接指定程序执行地址，不需要转换。
  
###CPU状态和初始化
  
当写一个多处理器OS时，区分每个处理器私有的CPU状态和系统共享的全局状态是很重要的。kern/cpu.h定义里大部分CPU状态包括存储了每个CPU变量的struct CpuInfo。cpunum()总是返回调用它的CPU的ID,可以被用作像cpus数组的序列。另外，宏thiscpu是对当前CPU的struct CpuInfo的速记。  
  
下面是需要了解的CPU状态  
  
> + CPU内核栈
    因为多个CPU可以同时陷入内核，所以需要为每个处理器主板一个独立的内核栈来避免影响互相之间的执行。[percpu_kstack[NCPU][KSTKSIZE]保留了NCPU份内核栈的空间。
    Lab2里把bootstack引用的物理地址映射到BSP的内核栈，刚好在KSTACKOP下方。相似的，本实验映射每个CPU的内核栈到这个区域，附带保护页作为它们之间的缓冲区。CPU0的栈将从KSTACKTOP向下增长，CPU1的栈从CPU0的栈下方KSTKGAP个字节的地方开始，如此向后。inc/memlayout.h展示了映射布局。
  
> + CPU TSS和TSS描述符
    一个CPU任务状态分段（TSS）也是需要的，用来指明每个CPU的内核栈在哪里。CPUi的TSS被存在cpu[i].cpu_ts里，对应的TSS描述符定义在GDT的gdt[(GD_TSS0 >> 3) + i]里。定义在kern/trap.c的全局ts变量就不再有用了
  
> + CPU当前环境指针
    因为每个CPU可以同时运行不同的用户进程，重定义符号curenv来引用cpus[cpunum()].cpu_env(或者thiscpu->cpu_env),它指向了执行在当前CPU的当前进程。  
  
> + CPU系统寄存器
    所有的寄存器，包括系统寄存器，都是CPU私有的。因此，初始化这些寄存器的指令，例如lcr3(),ltr(),lgdt(),lidt()...，必须在每个CPU上执行一次。函数env_init_percpu()和trap_init_percpu()就是为了这个目的定义的。  
  
另外，如果在之前的实验里添加了任何额外的CPU状态或者执行了其他额外CPU式初始化（即设置新bit位到CPU寄存器里）来挑战问题，记得把它们赋值到每个CPU里。  
  
> + Exercise 3
    修改mem_init_mp()（在kern/pmap.c)来映射CPU的栈到KSTACKTOP，如inc/memlayout.h里展示的一样。每个栈的大小为KSTKSIZE字节加上KSTKGAP字节的无映射保护页。完成后要通过check_kern_pgdir()的新检查。
  
用percpu_kstacks[i]引用cpui的内核栈物理地址。cpui的内核地址从虚拟地址kstacktop_i = KSTACKTOP - i * (KSTKSIZE + KSTKGAP)向下增长，同时也像MP的内核栈一样分为工作栈区和保护页区，许可为内核可读写，用户无。  
```
static void
mem_init_mp(void)
{
	uint32_t i;
	uintptr_t kstacktop_i;

	for (i = 0; i < NCPU; i++)
	{
		kstacktop_i = KSTACKTOP - i * (KSTKGAP + KSTKSIZE);
		boot_map_region(kern_pgdir,
						kstacktop_i - KSTKSIZE,
						KSTKSIZE,
						PADDR(&percpu_kstacks[i]),
						PTE_W );
	}
	

}
```
check_kern_pgdir()测试通过
  
> + Exercise 4
    trap_init_percpu()(kern/trap.c)的代码初始化了BSP的TSS和TSS描述符表。它在Lab3中是有效的，但在运行其他CPU时会出错。修改代码使得它能够在所有CPU上有效（注意：不能再使用全局变量ts了）。
  
提示：  
使用宏thiscpu引用当前CPU的srtuct CpuInfo；  
当前CPU的ID用cpunum()或thiscpu->cpu_id来获得；  
用thiscpu->cpu_ts来引用当前CPU的TSS；  
用gdt[(GD_TSS0 >> 3) + i]来引用CPUi的TSS描述符表；  
在mem_init_mp()里已经映射过每个CPU的内核；  
初始化cpu_ts.ts_iomb来避免无授权的环境使用IO操作（0不是正确的值）。  
  
```
void
trap_init_percpu(void)
{
	size_t i = thiscpu->cpu_id;
	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	thiscpu->cpu_ts.ts_esp0 = KSTACKTOP - i * (KSTKGAP + KSTKSIZE);
	thiscpu->cpu_ts.ts_ss0 = GD_KD;
	thiscpu->cpu_ts.ts_iomb = sizeof(struct Taskstate);

	// Initialize the TSS slot of the gdt.
	gdt[(GD_TSS0 >> 3) + i] = SEG16(STS_T32A, (uint32_t) (&thiscpu->cpu_ts),
					sizeof(struct Taskstate) - 1, 0);
	gdt[(GD_TSS0 >> 3) + i].sd_s = 0;

	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(GD_TSS0 + sizeof(struct Segdesc) * i);

	// Load the IDT
	lidt(&idt_pd);
}
```
  
完成上面的练习后，在QEMU中以4CPU形式运行JOS，指令`make qemu CPUS=4 (or make qemu-nox CPUS=4)`，输出类似如下：  
  
```
...
Physical memory: 66556K available, base = 640K, extended = 65532K
check_page_alloc() succeeded!
check_page() succeeded!
check_kern_pgdir() succeeded!
check_page_installed_pgdir() succeeded!
SMP: CPU 0 found 4 CPU(s)
enabled interrupts: 1 2
SMP: CPU 1 starting
SMP: CPU 2 starting
SMP: CPU 3 starting
```
测试得到输出  
```
Physical memory: 131072K available, base = 640K, extended = 130432K
check_page_free_list() succeeded!
check_page_alloc() succeeded!
check_page() succeeded!
check_kern_pgdir() succeeded!
check_page_free_list() succeeded!
check_page_installed_pgdir() succeeded!
SMP: CPU 0 found 4 CPU(s)
enabled interrupts: 1 2
SMP: CPU 1 starting
SMP: CPU 2 starting
SMP: CPU 3 starting

```  
###锁定
  
目前在mp_main()里初始化AP后代码会悬停。在让AP进行下一步前，首先需要解决多个处理器同时运行内核代码的条件竞争问题。最简单的达到这个目的的方法是使用一个大内核锁。这个锁是一个单一全局锁，无论何时一个环境进入内核都会获得它，并在环境返回用户模式时释放。在这个模型中，用户模式环境可以正确地在任何可获得CPU上运行，但不会有多个环境同时运行在内核模式；其他尝试进入内核模式的环境被强制等待。  
  
kern/spinlock.h声明了大内核锁，名为kernel_lock。它也提供了lock_kernel()和unlock_kernel()，作为获得和释放这个锁的快捷方式。把这个大内核锁应用到四个地方：  
> + 在i386_init()中，BSP唤醒其他CPU前获取锁。
> + 在mp_main()中，在初始化AP后获得锁，然后调用sched_yield()来在这个AP上运行环境。
> + 在trap()中，当从用户模式陷入内核时获得锁。通过检查tf_cs的低位来确定陷入发生在用户模式还是内核模式。
> + 在env_run()中，在切换回用户模式前释放锁。不要太早或太晚，不然就会产生竞态或者死锁。
  
  
> + Exercise 5
    在上述描述位置使用大内核锁，通过在正确的位置调用lock_kernel（）和unlock_kernel()。
  
/kern/init.c的i386init（）加锁  
```
	// Starting non-boot CPUs
	lock_kernel()
	boot_aps();
```
mp_main()中加锁并调用sched_yield();  
```
void
mp_main(void)
{
	// We are in high EIP now, safe to switch to kern_pgdir 
	lcr3(PADDR(kern_pgdir));
	cprintf("SMP: CPU %d starting\n", cpunum());

	lapic_init();
	env_init_percpu();
	trap_init_percpu();
	xchg(&thiscpu->cpu_status, CPU_STARTED); // tell boot_aps() we're up

	lock_kernel();
	sched_yield();
	// Remove this after you finish Exercise 6
	for (;;);
}
```  
  
trap()中加锁  
```
	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		// Acquire the big kernel lock before doing any
		// serious kernel work.
		// LAB 4: Your code here.
		lock_kernel();
		assert(curenv);
```  
env_run()中解锁  
```
    lcr3(PADDR(curenv->env_pgdir));
	unlock_kernel();
    env_pop_tf(&curenv->env_tf);
```  


现在还不能测试锁是否正确，直到完成了下个练习的调度架构。  
  
> + Question
    2.使用大内核锁看起来保证了同一时间只有一个CPU能够运行内核代码。为什么还需要给每个CPU分离的内核栈？描述一个甚至在有大内核锁的保护时,使用共享内核栈会出错的情景。
  
使用共享内核栈会修改别的CPU操作的数据，会导致别的CPU需要的数据丢失。
  
###Round-Robin调度
  
本实验的下个任务时改变JOS内核来使得它能够在多个环境间切换，以“round-robin”风格。JOS的Round-Robin调度以下列方式工作：  
  
> + kern/sched.c的函数sched_yield(）赋值选择新的环境来运行。它以持续循环搜索envs[]数组，从前一个正在运行的环境之后开始（如果之前没有运行的环境就从数组头部开始），选出它找到的第一个ENV_RUNNALBE(见inc/env.h)状态的环境，然后调用env_run（）来跳到那个环境。
> + sched_yield()一定不能同时在两个CPU上运行同一个环境。它通过环境状态是否为ENV_RUNNING来区分一个环境是否正在某个CPU上运行。
> + 现在已经实现了一个新系统调用sys_yield()，用户环境可以调用它来申请内核的sched_yield()函数并主动放弃CPU给到其他的环境。  
  
> + Exercise 6
    在sched_yield里实现上面描述的round-robin调度。不要忘记修改syscall()来分发sys_yield（）函数。
    确认在mp_main里申请sched_yield。
    修改kenr/init.c来创建三个（或更多）环境来运行程序user/yield.c。
    运行make qemu，现在应该能看到环境在终止前互相间切回切出五次，如下： 
    多cpu运行： make qemu CPUS=2
    ...
	Hello, I am environment 00001000.
	Hello, I am environment 00001001.
	Hello, I am environment 00001002.
	Back in environment 00001000, iteration 0.
	Back in environment 00001001, iteration 0.
	Back in environment 00001002, iteration 0.
	Back in environment 00001000, iteration 1.
	Back in environment 00001001, iteration 1.
	Back in environment 00001002, iteration 1.
	...
	在yield程序退出后，系统里没有可运行环境，调度器应该申请JOS内核监控器。在继续前完成上述效果。
  
  
sched_yield的round-robin实现：  
```
void
sched_yield(void)
{
	struct Env *idle;
	uint32_t i, j ,start;
	idle = thiscpu->cpu_env;
	
	start = (idle != NULL) ? ENVX(idle->env_id):0;
	
	for ( i = 0; i < NENV; i++)
	{
		j = (start + i) % NENV;
		if (envs[j].env_status == ENV_RUNNABLE)
		{
			env_run(&envs[j]);
			return;
		}
	}

	if (idle && idle->env_status == ENV_RUNNING)
	{
		env_run(idle);
		return;
	}	

	// sched_halt never returns
	sched_halt();
}
```
syscall()中添加case:  
```
		case (SYS_yield):
			sys_yield();
			return 0;
```
kern/init.c创建环境：  
```
void
i386_init(void)
{
 ...
	// Touch all you want.
	//ENV_CREATE(user_primes, ENV_TYPE_USER);
	ENV_CREATE(user_yield, ENV_TYPE_USER);
	ENV_CREATE(user_yield, ENV_TYPE_USER);
	ENV_CREATE(user_yield, ENV_TYPE_USER);
```
得到输出：  
```
Hello, I am environment 00001000.
Hello, I am environment 00001001.
Back in environment 00001000, iteration 0.
Hello, I am environment 00001002.
Back in environment 00001001, iteration 0.
Back in environment 00001000, iteration 1.
Back in environment 00001002, iteration 0.
Back in environment 00001001, iteration 1.
Back in environment 00001000, iteration 2.
Back in environment 00001002, iteration 1.
Back in environment 00001001, iteration 2.
Back in environment 00001000, iteration 3.
Back in environment 00001002, iteration 2.
Back in environment 00001001, iteration 3.
Back in environment 00001000, iteration 4.
Back in environment 00001002, iteration 3.
All done in environment 00001000.
[00001000] exiting gracefully
[00001000] free env 00001000
Back in environment 00001001, iteration 4.
Back in environment 00001002, iteration 4.
All done in environment 00001001.
All done in environment 00001002.
[00001001] exiting gracefully
[00001001] free env 00001001
[00001002] exiting gracefully
[00001002] free env 00001002
No runnable environments in the system!
Welcome to the JOS kernel monitor!

```  
  
> + Question
    3.在env_run()中已经调用过lcr3()。在调用lcr3()之前之后，代码引用了env_run的参数变量e（至少应该）。一旦加载%cr3寄存器，MMU使用的寻址文段立刻被切换。但一个虚拟地址（叫e）有和给定地址文段相关的意义--指明了虚拟地址映射的物理地址。为什么指针e可以在地址切换前后能被解引用？
    4.无论何时当内核从一个环境切换到另一个，它一定确保了旧环境的寄存器被保存使得它们之后能被正确地恢复。为什么？这件事在哪里发生？
  
3.因为env_run()运行在内核态，能够访问所有环境的页表映射，切换当前页表不会改变之前的映射。
  
4.在trap中发生。trap实际处理前将当前栈中的陷入帧保存在环境的tf记录中，使得它可以在陷入点恢复。  
  
###环境创建系统调用
  
尽管内核现在有能力运行和切换多用户等级环境，它仍然限制于运行内核一开始设置的环境，限制实现重要的JOS系统调用来允许用户环境来创建和开始其他的用户环境。  
  
Unix提供了fork()系统调用作为它的进程创建原型。Unix fork()拷贝了整个调用进程（父进程）的地址空间来创建新进程（子进程）。在可观察的用户空间中它们唯一的区别是它们的进程Id（通过getpid和getppid获得）。父进程中,fork()返回子进程的进程ID，而在子进程中fork()返回0。默认地，每个进程有它自己地私有地址空间，而其他进程对内存地修改互相之间是不可见的。  
  
要提供一个不同的，更原始的一套JOS系统调用来创建用户模式环境。用这些系统调用就可以完全在用户空间里实现一个类Unix的fork()，作为对其他风格环境创建的补充。这个新系统调用有如下的部分：  
  
sys_exofork:  
  这个系统调几乎白板地创建了一个新环境：没有东西映射在地址空间的用户部分，也无法运行。新环境将拥有和父环境一样的寄存器状态，在sys_exofork调用时。父环境中，sys_exofork返回新创建环境的envid_t（或者一个负的错误码在环境分配失败时）。而在子环境中，它会返回0。（因为子环境开始标记为不可运行，sysexofork不会实际在子环境中返回直到父环境标记了子环境可运行后再显式地允许它返回。）  
sys_env_set_status:
  设置一个特定环境的状态为ENV_RUNNALBE或ENV_NOT_RUNNABLE。这个系统调用通常被用来标记新环境已经准备好运行了，一旦它的地址空间和寄存器状态被完全初始化后。  
sys_page_alloc:  
  分配一页物理内存并映射它到一个给定环境地址空间的给定虚拟地址。  
sys_page_map:  
  从一个环境的地址空间拷贝一个页映射（而不是页的内容）到另一个，留下一个共享的内存使得新和旧映射都指向同一个物理页内存。  
sys_page_unmap:  
  解除一个给定环境的给定虚拟地址的页映射。  
  
所有上述的系统调用接受环境的ID，JOS内核支持0值意味这当前环境的惯例。这个惯例再kern/env.c的envid2env()里实现。  
  
在user/dumpfork.c里提供了一个非常原始的类Unix fork（）实现。这个测试软件使用了上述的系统调用来创建和运行一个子环境，使用它自己地址空间的拷贝。这两个环境之后使用之前练习的sys_yield来切回和切出。父进程在10次迭代后退出，而在子进程中迭代20次后。  
  
> + Exercise 7
    在kern/syscall.c中实现上述的系统调用，并确认syscall()能调用它们。在kern/pamp.c和kern/env.c中使用多种函数，尤其是envid2env()。现在，无论何时调用envid2env()，传递1到checkperm参数里。确保检查对任何无效的系统调用参数，返回-E_INVAL，在最后的case中。用user/dumbfork来测试JOS内核，继续前确保通过。
  
sys_exofork(）用kern/env.c的env_alloc（）创建新环境。在env_alloc创建后就留下，除非被设置为不可运行，同时从当前环境拷贝寄存器状态，但稍稍修改使得在子环境中返回0。  
```
static envid_t
sys_exofork(void)
{
	struct Env *newenv;
	int ret;
	if ((ret = env_alloc(&newenv, sys_getenvid()))<0)
		return ret;
	newenv->env_status = ENV_NOT_RUNNABLE;
	newenv->env_tf = curenv->env_tf;
	newenv->env_tf.tf_regs.reg_eax = 0;
	return newenv->env_id;
}

```
  
sys_env_set_status 设置成功时返回0，错误时返回小于0，包括-E_BAD_ENV在环境id当前不存在或当前环境无改变许可时，-E_INVAL在设置的状态量非法时。  
提示：使用envid2env函数来转换envid和Env结构。设置envid2env的第三个参数为1，检查当前环境是否有设置envid状态的许可。  
```
static int
sys_env_set_status(envid_t envid, int status)
{
	if (status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE)
		return -E_INVAL;
	struct Env *tagetenv;
	int ret = envid2env(envid,&tagetenv,1);
	if (ret < 0)
		return ret;
	tagetenv->env_status = status;
	reutrn 0;
}

```
  
sys_page_alloc  分配一页的内存并映射它到envid地址空间的虚拟地址va，许可为perm。页内容设置为0。如果页已经被映射过了，那么原先的页映射作为副作用会被解除。perm的PTE_U | PTE_P必须被设置，PTE_AVAIL | PTE_W可以被设置，其余位不应该被设置，详见PTE_SYSCALL（inc/mmu.h）。成功时返回0，错误包括-E_BAD_ENV在环境id不存在或无访问许可，-E_INVAL在va地址高于UTOP或者不可分配和许可不正确时，-E_NO_MEM在无内存来分配新页或将映射到任何重要的页表时。  
提示：这个函数是对page_alloc()和page_insert()的封装。要检查参数的正确性。如果page_insert()失败了，释放已分配的页。  
```
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	if (va >= (void*)UTOP)
		return -E_INVAL;
	if ((perm & PTE_U) == 0 || (perm & PTE_P) == 0)
		return -E_BAD_ENV;
	if ((perm & ~(PTE_U | PTE_P | PTE_W |PTE_AVAIL)) != 0)
		return -E_NO_MEM;
	struct Env * e;
	if (envid2env(envid,&e,1) < 0)
		return -E_BAD_ENV;
	struct PageInfo *p = page_alloc(0);
	if (!p)
		return -E_NO_MEM;
	if (page_insert(e->env_pgdir, p, va, perm) < 0)
	{
		page_free(p);
		return -E_NO_MEM;
	}
	memset(page2kva(p), 0, PGSIZE);
	return 0;			
}
```
  
sys_page_map  映射srcenvi地址空间的srcva的内存页到dstenvid地址空间的dstva处，许可为perm。perm有着和sys_page_alloc一样的限制，此外它还不能写入一个只读页。  
成功时返回0，错误包括：-E_BAD_ENV在环境id不存在或无访问许可，-E_INVAL在srcva/dstva地址高于UTOP或者不可分配;许可不正确;scva没有映射到srdenvid地址空间；许可有可写，而在srenvid的srva是只读时，-E_NO_MEM在无内存来分配新页或将映射到任何重要的页表时。  
提示：这个函数是对page_look()和page_insert()的封装。要检查参数的正确性。用page_look()的第三个参数来当前对页的许可。  
```
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	struct Env *srcenv, *dstenv;
    struct PageInfo *p;
    pte_t *pte;

    if (envid2env(srcenvid, &srcenv, 1) < 0 || envid2env(dstenvid, &dstenv, 1) < 0)
        return -E_BAD_ENV;
    if ((uintptr_t)srcva >= UTOP || PGOFF(srcva) || (uintptr_t)dstva >= UTOP || PGOFF(dstva))
        return -E_INVAL;
    if ((perm & PTE_U) == 0 || (perm & PTE_P) == 0 || (perm & ~PTE_SYSCALL) != 0)
        return -E_INVAL;
    if ((pp = page_lookup(srcenv->env_pgdir, srcva, &pte)) == NULL)
    	return -E_INVAL;
    if ((perm & PTE_W) && (*pte & PTE_W) == 0)
        return -E_INVAL;
    if (page_insert(dstenv->env_pgdir, p, dstva, perm) < 0)
        return -E_NO_MEM;
    return 0;

}

```
  
sys_page_unmap  解除envid地址空间va处的页映射。如果没有页分配则什么都不做。  
成功时返回0，错误包括：-E_BAD_ENV在环境id不存在或无访问许可时。-E_INVAL在va地址高于UTOP或者不可分配和许可不正确时。  
提示：这个函数是对page_remove（）的封装。  
```
static int
sys_page_unmap(envid_t envid, void *va)
{
    struct Env *e;

    if (envid2env(envid, &e, 1) < 0)
        return -E_BAD_ENV;
    if ((uintptr_t)va >= UTOP || PGOFF(va))
        return -E_INVAL;

    page_remove(e->env_pgdir, va);
    return 0;
}
```
sys_call()中添加case  
```
		case (SYS_exofork):
			return sys_exofork();
		case (SYS_env_set_status):
			return sys_env_set_status(a1, a2);
		case (SYS_page_alloc):
			return sys_page_alloc(a1, (void *)a2, a3);
		case (SYS_page_map):
			return sys_page_map(a1,(void *)a2, a3, (void *)a4,a5);
		case (SYS_page_unmap):
			return sys_page_unmap(a1, (void *)a2);
```  
运行指令 `make run-dump-nox`得到输出：
```
[00000000] new env 00001000
[00001000] new env 00001001
0: I am the parent!
0: I am the child!
1: I am the parent!
1: I am the child!
2: I am the parent!
2: I am the child!
3: I am the parent!
3: I am the child!
4: I am the parent!
4: I am the child!
5: I am the parent!
5: I am the child!
6: I am the parent!
6: I am the child!
7: I am the parent!
7: I am the child!
8: I am the parent!
8: I am the child!
9: I am the parent!
9: I am the child!
[00001000] exiting gracefully
[00001000] free env 00001000
10: I am the child!
11: I am the child!
12: I am the child!
13: I am the child!
14: I am the child!
15: I am the child!
16: I am the child!
17: I am the child!
18: I am the child!
19: I am the child!
[00001001] exiting gracefully
[00001001] free env 00001001
No runnable environments in the system!
```
测试通过