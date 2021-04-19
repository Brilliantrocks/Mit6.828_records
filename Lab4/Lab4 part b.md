# Lab4
## Part b 写时拷贝Fork
  
就像之前提到的，Unix提供了fork（）系统调用作为它主要的进程创建原型。fork()系统调用拷贝了调用（父进程）进程的地址空间来创建一个新进程（子进程）。
  
xv6 Unix通过拷贝所有父环境的页数据到子环境里来实现fork()。这就是dumbfork（）采取的方法。拷贝父环境的地址空间到子环境是这个fork操作最昂贵的部分。  
  
然而在子进程中频繁发生fork()在调用后立刻调用了exec()的情况，用新程序替换子进程的内存。这就是shell通常做的。这种情况下，拷贝父进程地址空间的时间被大量浪费了，因为子进程在调用exec()前只会使用内存中很少的一部分。  
  
因此，Unix后续版本利用了虚拟内存硬件来允许父进程和子进程来共享内存映射到各自的地址空间只读一个进程实际修开了它。这个技术被称为写时拷贝。为了完成这件事，一旦fork()时，内核会拷贝父进程的地址空间映射到子进程中，而不是映射页的内容，同时标记实时共享的页为只读。当两个进程之一尝试写入共享页时，进会产生一个页错误。此刻，Unix内核知道了这个页是真正“虚拟”的或者写时拷贝的，然后它会制作出一个新的，私有的，可写的页拷贝给出错的进程。这种方式各个页的内容不会实际拷贝直到它们实际需要写入时。这个优化使得子进程中紧接着exec()的fork（）操作更划算：子进程在它调用exec()前可能只需要拷贝一页（它栈中的当前页）。  
  
在下面的实验中，实现一个正确的类Unix fork()，使用写时拷贝技术，作为一个用户库方法。实现fork()和用户空间写时拷贝支持有着内核恢复简单得多的从而更可能得到正确结果的好处。它页使得独立的用户模式程序定义它们自己的fork语义。一个想要稍微不同实现的程序（例如，dumbfork这样昂贵的全拷贝版本，或者父进程和子进程之后实际共享内存的程序）可以简单地自己完成。  
  
### 用户等级页错误处理
一个用户等级写时拷贝fork()需要知道写入保护页的页错误，所以这是要实现的第一个事。写时拷贝只是用户等级页错误处理多种可能中的一种。  
  
设置一个地址空间时页错误是很常见的，它暗示需要采取一些动作。例如，大多Unix内核初始只映射单页到处理器的栈区，之后当处理器栈消耗增加在尚未映射的栈地址引起页错误时再分配和映射额外的栈页。处理器空间各个区域里发生页错误时，一个典型的Unix内核一定要保持对将要采取的行为的追踪。例如，一个栈区错误通常会分配和映射一个新物理页。一个程序的BSS区域错误会分配一个新页，填充0，然后映射它。在可执行需求分页的系统中，一个下一个文段区域的错误会从二进制文件中读取对应的页再映射它。  
  
这有很多内核需要保持追踪的信息。作为传统Unix方法的替代，需要决定对每个用户空间页错误做些什么，这里bug会造成更小的伤害。这种设计有允许程序定义它们内存区域来获得巨大灵活性的好处；之后要使用用户等级页错误处理来映射和访问基于硬盘的文件系统上的文件。  
  
#### 设置页错误处理程序
  
为了处理它自己的页错误，一个用户环境需要注册一个页错误处理程序进入点到JOS内核。用户环境通过新sys_env_set_pgfault_upcall系统调用来注册它的页错误进入点。Env结构中已经增加了一个新成员en_pgfault_upcall来记录这个信息。  
  
> + Exercise8 
    实现sys_env_set_pgfault_upcall系统调用。确保在查找目标环境的环境ID时开启许可检查，因为这是一个危险的系统调用。  
  
sys_env_set_pgfault_upcall  修改envid对应Env结构的env_pgfault_upcall字段来设置页错误调用。当envid产生页错误时，压入一个错误记录到异常栈，之后创建分支执行func。  
成功时返回0，错误包括-E_BAD_ENV在环境envid不存在或者调用者无改变envid的许可时。
```
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	struct Env *e;
	if (envid2env(envid, &e, 1) < 0)
		return -E_BAD_ENV;
	e->env_pgfault_upcall = func;
	return 0;
}

```  
syscall()中添加case：  
```
		case (SYS_env_set_pgfault_upcall):
			return sys_env_set_pgfault_upcall(a1, (void *)a2);
```  

#### 用户环境的普通和异常栈
  
在普通执行期间，一个JOS内的用户环境会运行在普通用户栈：它的ESP寄存器开始指向USTACKTOP，它压入的栈数据放在USTACKTOP-PGSIZE和USTACKTOP-1之间保护的页上。而当用户模式发生一个页错误，内核会重启用户环境来运行一个指定的用户等级页错误处理函数到一个不同的栈，叫用户异常栈。实际上，要使得JOS内核代表用户环境实现自动栈切换，就像x86处理器已经实现的在用户模式切换至内核模式时代表JOS来切换栈一样。  
  
JOS用户异常栈也是一页大小，它的顶部被定义在虚拟地址UXSTACKTOP，所以有效的用户异常栈包含在UXSTACKTOP-PGSIZE到UXSTACKTOP-1内。当在这个异常栈上运行时，用户等级页错误处理程序可以使用JOS的标准系统调用来映射新页或者调整映射来修正原先导致这个页错误的问题。然后用户等级页错误处理函数返回，通过一个汇编语言桩代码，到原先栈中出错的代码。  
  
每个想要支持用户等级页错误处理的用户环境需要为它自己的异常栈分配内存，使用part a 中sys_page_alloc()系统调用。  
  
#### 申请用户页错误处理函数
  
现在需要改变kern/trap.c的页错误处理代码来处理用户模式的页错误。把这个错误时用户环境的状态称为陷入时状态。  
  
如果没有注册的页错误处理函数，JOS内核会销毁用户环境并发送一个消息，就像之前一样。否则，内核设置一个陷入帧到异常栈，就像inc/trap.h的UTrapframe结构一样：  
```
                    <-- UXSTACKTOP
trap-time esp
trap-time eflags
trap-time eip
trap-time eax       start of struct PushRegs
trap-time ecx
trap-time edx
trap-time ebx
trap-time esp
trap-time ebp
trap-time esi
trap-time edi       end of struct PushRegs
tf_err (error code)
fault_va            <-- %esp when handler is run
```
  
内核之后安排用户环境恢复执行，页错误处理函数用这个栈帧运行在异常栈上；一定要理解如何让这件事发送。fault_va 是导致页错误的虚拟地址。  
  
如果用户环境已经运行在异常栈上而又发生了异常，那么页错误处理函数自身出错了。这种情况下，应该在当前tf->tf_esp下面开始一个新栈而不是在UXSTACKTOP。首先压入空32位词节，然后是一个UTrapframe结构。  
  
为了测试tf->tf_esp是否已经在用户异常栈上，检查它是否在UXSTACKTOP-PGSIZE和UXSTACKTOP-1之间。  
  
> + Exercise9 
	实现kern/trap.c里page_fault_handler的代码，分发页错误到用户模式处理函数。确保写入异常栈时采取了正确的预防措施。（如果用户环境用光了异常栈会发生什么？）
  
之前Lab3已经处理了内核模式异常，之后的代码处理用户模式页错误。  
  
调用环境的页错误调用，如果有的话。设置一个页错误栈帧到异常栈上（在UXSTACKTOP之下），然后开启分支执行curenv->env_pgfault_upcall。  
  
页错误调用可能会引起另一个页错误调用，这种情况下递归地调用页错误调用，压入另一个页错误栈帧到用户异常栈顶。  
  
让从页错误返回的代码在陷入时栈的顶部拥有一个词节的小空间会很方便；这允许更简单地恢复eip/esp。在非递归的情况下，不用担心这种情况因为常规用户栈顶还是可用的。在递归状况下，这意味着需要在当前异常栈顶和新栈帧间保留额外一词节的空间，因为异常栈就是陷入时栈。  
  
如果没有页错误调用，环境没有分配它的异常栈一页或者不能写入它，或者异常栈溢出了，那么销毁导致错误的环境。注意打分脚本假设你首先检查了页错误调用并在没有时打印用户错误地址信息。  
 
提示：user_mem_assert()和env_run()很有用。修改curenv->env_tf来改变用户环境运行。（tf变量指向curenv->env_tf。）  
  
添加代码  
```
	struct UTrapframe *utf;

	if (curenv->env_pgfault_upcall) {
		if (UXSTACKTOP - PGSIZE <= tf->tf_esp && tf->tf_esp <= UXSTACKTOP - 1) 
			utf = (struct UTrapframe *)(tf->tf_esp - sizeof(struct UTrapframe) - 4);
		else 
			utf = (struct UTrapframe *)(UXSTACKTOP - sizeof(struct UTrapframe));
		user_mem_assert(curenv, (void *)utf, sizeof(struct UTrapframe), PTE_U | PTE_W);

		utf->utf_fault_va = fault_va;
		utf->utf_err = tf->tf_trapno;
		utf->utf_eip = tf->tf_eip;
		utf->utf_eflags = tf->tf_eflags;
		utf->utf_esp = tf->tf_esp;
		utf->utf_regs = tf->tf_regs;
		tf->tf_eip = (uint32_t)curenv->env_pgfault_upcall;
		tf->tf_esp = (uint32_t)utf;
		env_run(curenv);
	}

```
当溢出时，内核没有之后的内存空间访问权限，则会触发内核模式的页错误，报出错误信息。  
  
#### 用户模式页错误进入点
  
接下来，需要实现关心调用C页错误处理函数和在错误指令处恢复执行的汇编方法。这个汇编路径是被内核用sys_env_set_pgfault_upcall()注册的处理函数。  
  
> + Exercise 10 实现lib/pfentry.S里的_pgfault_upcall方法。有意思的部分是返回原先用户代码引起页错误的点。将直接返回哪里，而不用回到内核。难点在于同时切换栈和重加载EIP。
  
pdentry.S  这是要求内核在用户空间引起页错误时重定向的地方。
  
当以一个页错误实际发生，内核切换ESP到指向用户异常栈（如果不在异常栈的话）。然后向里面压入一个UTrapframe。  
  
异常栈：
// exception stack:
//
//	trap-time esp
//	trap-time eflags
//	trap-time eip
//	utf_regs.reg_eax
//	...
//	utf_regs.reg_esi
//	utf_regs.reg_edi
//	utf_err (error code)
//	utf_fault_va            <-- %esp
  
如果这是一个递归的错误，内核会保留一个空白词节在陷入时esp之上，方便递归调用返回。  
  
然后调用C语言码中合适的页错误处理函数，由_pgfault_handler指向。  
  
必须为最终返回和重新执行错误指令提前做准备。然而不幸的是，不能直接从异常栈返回：无法调用jmp，因为这需要加载地址到寄存器里，而所有寄存器在返回后都有它们的陷入时值。  
也不能从异常栈里调用ret，因为如果这么做了，%esp会有错误的值。  
所以作为替代，压入陷入时%eip到陷入时栈里。  
接下来要切换至那个栈并调用'ret'，恢复%eip到它之前错误的值。  
考虑到异常栈的递归错误，注意现在压入的值会填在内核为我们保留的空白词节上。  
回顾整个恢复代码，仔细想想哪些寄存器可以用来计算立即数。不要使用在小缺口空间不可用的寄存器。  

```
.text
.globl _pgfault_upcall
_pgfault_upcall:
	// Call the C page fault handler.
	pushl %esp			// function argument: pointer to UTF
	movl _pgfault_handler, %eax
	call *%eax
	addl $4, %esp			// pop function argument
	
    movl 48(%esp), %ebp/*  *ebp = *(esp + 48) 获得陷入时esp */
    subl $4, %ebp/* *ebp = *ebp - 4 获得保留空间地址或者用户栈顶*/
    movl %ebp, 48(%esp)/* *(esp + 48) = *ebp  */
    movl 40(%esp), %eax/* *eax = *(esp + 40) 获得陷入时eip */
    movl %eax, (%ebp)/* *ebp = *eax 放入保留空间 */
	// Restore the trap-time registers.  After you do this, you
	// can no longer modify any general-purpose registers.
	// LAB 4: Your code here.
    addl $8, %esp/* 获得陷入时寄存器 */
    popal/* 恢复 */
	// Restore eflags from the stack.  After you do this, you can
	// no longer use arithmetic operations or anything else that
	// modifies eflags.
	// LAB 4: Your code here.
    addl $4, %esp/* 获得陷入时eflga标准位寄存器 */
    popfl/* 恢复 */
	// Switch back to the adjusted trap-time stack.
	// LAB 4: Your code here.
    popl %esp /* 恢复至陷入时esp */
	// Return to re-execute the instruction that faulted.
	// LAB 4: Your code here.
	ret

```  
  
最后，需要实现C用户库侧的用户等级页错误处理机制。  
  
> + Exercise11 完成lib/pgfault.c里的set_pgfault_handler()。  
    
set_pgfault_handler 设置页错误处理函数，如果不存在即为0。第一次注册一个处理函数时，需要分配一个异常栈（一页栈顶在UXSTACKTOP的内存），然后在页错误发生时告诉内核调用汇编方法_pgfault_upcall。  
  
```
void
set_pgfault_handler(void (*handler)(struct UTrapframe *utf))
{
void
set_pgfault_handler(void (*handler)(struct UTrapframe *utf))
{
	int r;

	if (_pgfault_handler == 0) {
		// First time through!
		// LAB 4: Your code here.
		if ((r = sys_page_alloc(thisenv->env_id,(void*)(UXSTACKTOP - PGSIZE),PTE_P | PTE_W | PTE_U))
		       < 0)
			panic("set_pgfault_handler failed: %e ",r);
		sys_env_set_pgfault_upcall(thisenv->env_id, _pgfault_upcall);
		//panic("set_pgfault_handler not implemented");
	}

	// Save handler pointer for assembly to call.
	_pgfault_handler = handler;
}
```
  
#### 测试
  
运行`make run-faultread-nox`  
  
得到输出：  
```
[00000000] new env 00001000
[00001000] user fault va 00000000 ip 00800039
TRAP frame at 0xf02b0000 from CPU 0
  edi  0x00000000
  esi  0x00000000
  ebp  0xeebfdfd0
  oesp 0xefffffdc
  ebx  0x00000000
  edx  0x00000000
  ecx  0x00000000
  eax  0xeec00000
  es   0x----0023
  ds   0x----0023
  trap 0x0000000e Page Fault
  cr2  0x00000000
  err  0x00000004 [user, read, not-present]
  eip  0x00800039
  cs   0x----001b
  flag 0x00000086
  esp  0xeebfdfc0
  ss   0x----0023
[00001000] free env 00001000
```  
  
运行user/faultdie，得到输出：  
```  
enabled interrupts: 1 2
[00000000] new env 00001000
i faulted at va deadbeef, err 6
[00001000] exiting gracefully
[00001000] free env 00001000
No runnable environments in the system!

```
  
运行user/faultalloc，得到输出：  
```
[00000000] new env 00001000
fault deadbeef
this string was faulted in at deadbeef
fault cafebffe
fault cafec000
this string was faulted in at cafebffe
[00001000] exiting gracefully
[00001000] free env 00001000
No runnable environments in the system!
```
  
运行user/faultallocbad，得到输出：  
```
[00000000] new env 00001000
[00001000] user_mem_check assertion failure for va deadbeef
[00001000] free env 00001000
No runnable environments in the system!
````
  
确认理解为何faultalloc和faultallocbad出现不同的表现。  
faultallocbad中  
`sys_cputs((char*)0xDEADBEEF, 4);`  
直接调用了系统调用来输出一个为分配的页，触发函数内部的警告。  
faultalloc中
`cprintf("%s\n", (char*)0xDeadBeef);`  
会先触发页错误，进而陷入内核态，经过page_fault_handler处理分配页，从而正常访问地址。  
  
### 实现写时拷贝  
  
现在内核有能力实现完全在用户空间的写时拷贝fork()了。  
  
在lib/fork.c中提供了一个fork()框架。就像dumbfork()，fork（）应该创建一个新环境，然后扫描父环境的整个地址空间，在子环境中设置对应的页映射。关键的不同在于，dumbfork()拷贝页，fork（)最初只拷贝页映射。fork()将只在一个环境尝试写入时才拷贝页。  
  
fork()的基础控制流如下：  
  
  1.父环境安装pgfault()作为C语言等级的页错误处理，使用之前实现的set_pgfault_handler()函数。
  
  2.父环境调用sys_exofork() 来创建子环境。
  
  3.对每个地址在UTOP之下，可写或者写时拷贝的页，父进程调用duppage，映射写时拷贝页到子环境地址空间，然后重映射写时拷贝页到它自己的地址空间。[注意：这里的顺序（例如，先在子环境中标记一个页为写时拷贝，再在父环境中标记为写时拷贝）非常重要。为什么？尝试想象一种反转顺序引起麻烦的具体情况。]duppage设置PTEs使得页不可写，填充PTE_COW到avail字段来区分写时拷贝页和真正的只读页。
  然而异常栈不会以这种方式重映射。作为替代需要为了异常栈分配一个新页到子环境中。因为页错误处理函数会完成实际拷贝且运行在异常栈上，异常栈不能写时拷贝：谁会拷贝它呢？  
  fork()同样需要处理存在但不可写或者写时拷贝的页。  
  
  4.父环境为子环境设置用户页错误进入点，就像为它自己的那样。  
  
  5.子环境现在准备好运行了，所以父环境标记它为可运行。  
  
用户等级lib/fork.c代码为上述操作必须查询环境页表（例如，标记一个页的PTE为PTE_COW）。内核映射环境页表到UVPT就是为了这个目的。它使用了一种[聪明的映射技巧](https://pdos.csail.mit.edu/6.828/2018/labs/lab4/uvpt.html)来让用户代码检查PTEs更简单。lib/entry.S设置uvpt和uvpd来使得lib/fork.c可以简单地检查页表信息。  
  
> + Exercise 12  
    实现lib/fork.c中的fork,duppage和pgfault。
	用forktree程序来测试代码。它应该产生下面的信息，夹带着 'new env', 'free env', 和'exiting gracefully'的信息。信息也许不会以这种顺序出现，环境ID也可能不同。  
	1000: I am ''
	1001: I am '0'
	2000: I am '00'
	2001: I am '000'
	1002: I am '1'
	3000: I am '11'
	3001: I am '10'
	4000: I am '100'
	1003: I am '01'
	5000: I am '010'
	4001: I am '011'
	2002: I am '110'
	1004: I am '001'
	1005: I am '111'
	1006: I am '101'
  
pgfault（）如果错误页为写时拷贝时定制页错误处理函数	。映射私有可写得拷贝。检查错误访问，要求1.可写错误，2.到写时拷贝页	；否则报错。  
提示：用映射在uvpt的只读页表（见inc/memlayout.h）。  
分配一个新页，映射它到一个临时的地方（PFTEMP),从旧页向新页拷贝数据，然后移动新页到旧页的地址。
提示：三次系统调用。
```
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;
	if ((err & FEC_WR) == 0 || (uvpt[PGNUM(addr)] & PTE_COW) == 0)
		panic("pgfault: it's not writable or attempt to access a non-cow page!");

	envid_t envid = sys_getenvid();
	if ((r = sys_page_alloc(envid, (void *)PFTEMP, PTE_P | PTE_W | PTE_U)) < 0)
		panic("pgfault: page allocation failed %e", r);

	addr = ROUNDDOWN(addr, PGSIZE);
	memmove(PFTEMP, addr, PGSIZE);
	if ((r = sys_page_unmap(envid, addr)) < 0)
		panic("pgfault: page unmap failed %e", r);
	if ((r = sys_page_map(envid, PFTEMP, envid, addr, PTE_P | PTE_W |PTE_U)) < 0)
		panic("pgfault: page map failed %e", r);
	if ((r = sys_page_unmap(envid, PFTEMP)) < 0)
		panic("pgfault: page unmap failed %e", r);
}
```  
duppage() 映射虚拟页pn（地址为pn * PGSIZE)到目标envid同样的虚拟地址。如果页是可写或者写时拷贝的，新映射页也要作为写时拷贝页创建。成功时返回0，错误时<0。
```
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	void *addr;
    pte_t pte;
    int perm;

    addr = (void *)((uint32_t)pn * PGSIZE);
    pte = uvpt[pn];
    perm = PTE_P | PTE_U;
    if ((pte & PTE_W) || (pte & PTE_COW))
        perm |= PTE_COW;
    if ((r = sys_page_map(thisenv->env_id, addr, envid, addr, perm)) < 0) {
        panic("duppage: page remapping failed %e", r);
        return r;
        }
    if (perm & PTE_COW) {
        if ((r = sys_page_map(thisenv->env_id, addr, thisenv->env_id, addr, perm)) < 0) {
            panic("duppage: page remapping failed %e", r);
            return r;
            }
    	}
	return 0;
}
```
    
fork（）  用户等级写时拷贝fork，正确设置页错误处理函数，创建子环境，拷贝地址空间和设置给子环境的页错误处理函数。然后标记子环境为可运行并返回。  
  
返回值：父环境为子环境的envid，子环境为0，小于0时错误，可以在错误时报错。  
提示：使用uvpd，uvpt和duppage。记得在子环境中修正thisenv。用户异常栈永远不会被标记为写时拷贝，所以必须为子环境y用户异常栈分配一个新页。  
   
```
envid_t
fork(void)
{
	// LAB 4: Your code here.
	envid_t envid;
	uint32_t addr;
	int i, j, pn, r;
	extern void _pgfault_upcall(void);

	set_pgfault_handler(pgfault);
	if ((envid = sys_exofork()) < 0) {
		panic("sys_exofork failed: %e", envid);
		return envid;
	}
	if (envid == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	for (i = PDX(UTEXT); i < PDX(UXSTACKTOP); i++) {
		if (uvpd[i] & PTE_P) {
			for (j = 0; j < NPTENTRIES; j++) {
				pn = PGNUM(PGADDR(i, j, 0));
				if (pn == PGNUM(UXSTACKTOP - PGSIZE))
					break;
				if (uvpt[pn] & PTE_P)
					duppage(envid, pn);
			}
		}
	
	}
	
	if ((r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W)) < 0) {
		panic("fork: page alloc failed %e", r);
		return r;
	}
	if ((r = sys_page_map(envid, (void *)(UXSTACKTOP - PGSIZE), thisenv->env_id, PFTEMP, PTE_P | PTE_U | PTE_W)) < 0) {
		panic("fork: page map failed %e", r);
		return r;
	}
	memmove((void *)(UXSTACKTOP - PGSIZE), PFTEMP, PGSIZE);
	if ((r = sys_page_unmap(thisenv->env_id, PFTEMP)) < 0) {
		panic("fork: page unmap failed %e", r);
		return r;
	}
	sys_env_set_pgfault_upcall(envid, _pgfault_upcall);
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0) {
		panic("fork: set child env status failed %e", r);
		return r;
	}

	return envid;
}
```  
运行forktree，得到输出：
```
[00000000] new env 00001000
1000: I am ''
[00001000] new env 00001001
[00001000] new env 00001002
[00001000] exiting gracefully
[00001000] free env 00001000
1001: I am '0'
[00001001] new env 00002000
[00001001] new env 00001003
[00001001] exiting gracefully
[00001001] free env 00001001
2000: I am '00'
[00002000] new env 00002001
[00002000] new env 00001004
[00002000] exiting gracefully
[00002000] free env 00002000
2001: I am '000'
[00002001] exiting gracefully
[00002001] free env 00002001
1002: I am '1'
[00001002] new env 00003001
[00001002] new env 00003000
[00001002] exiting gracefully
[00001002] free env 00001002
3000: I am '11'
[00003000] new env 00002002
[00003000] new env 00001005
[00003000] exiting gracefully
[00003000] free env 00003000
3001: I am '10'
[00003001] new env 00004000
[00003001] new env 00001006
[00003001] exiting gracefully
[00003001] free env 00003001
4000: I am '100'
[00004000] exiting gracefully
[00004000] free env 00004000
2002: I am '110'
[00002002] exiting gracefully
[00002002] free env 00002002
1003: I am '01'
[00001003] new env 00003002
[00001003] new env 00005000
[00001003] exiting gracefully
[00001003] free env 00001003
5000: I am '011'
[00005000] exiting gracefully
[00005000] free env 00005000
3002: I am '010'
[00003002] exiting gracefully
[00003002] free env 00003002
1004: I am '001'
[00001004] exiting gracefully
[00001004] free env 00001004
1005: I am '111'
[00001005] exiting gracefully
[00001005] free env 00001005
1006: I am '101'
[00001006] exiting gracefully
[00001006] free env 00001006
No runnable environments in the system!
```
