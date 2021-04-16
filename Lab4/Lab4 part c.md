#Lab 4
  
##Part c 抢占式多任务和进程间通信
  
在实验4的最后，修改内核来抢占非合作性环境，并允许环境间显式的消息传递。  
  
###时钟中断和抢占 
  
运行user/spin测试程序。这个测试程序fork了一个子环境，，一旦它获得了CPU的控制，就简单地在死循环中永远悬停。无论时内核还是父环境都不能重新获得CPU。这陷入不是一个理想的状况，从保护系统远离用户空间bug和恶意代码的角度来看，因为所有用户环境都能通过一个无限循环而不交回CPU来让整个系统宕机。为了允许内核抢占一个正在运行的环境，从它那里强制重获CPU的控制权，我们必须扩展JOS内核来从时钟硬件支持外部硬件中断。  
  
####中断准则
  
外部中断（如设备中断）也称作IRQ。有16种可能的IRQ，从0-15。IRQ在IDT中的映射不是固定的。picirq.c的pic_init映射了IRQ 0-15到IDT记录的IRQ_OFFSET - IRQ_OFFSET+15。  
  
在inc/trap.h，IRQ_OFFSET被定义为十进制数32.因此IDT 32-47对应着IRQ 的0-15。例如，时钟中断为IRQ 0。因此IDT[IRQ_OFFSET+0] (如IDT[32])包含了内核时钟中断处理方法的地址。IRQ_OFFSET是被选定的，因此设备中断不会和处理器异常重叠，那显然会造成混淆。（事实上，在早期计算机运行MS-DOS的日子里，IRQ_OFFSET就曾是0，导致在处理硬件中断和处理处理器异常时造成巨大的混淆。)  
  
在JOS里，相对于xv6做了一个关键的简化。外部设备中断在内核里永远关闭（而在xv6里在用户空间开启）。外部中断通过%eflags寄存器（见inc/mmu.h)的FL_IF标志位控制。当这个标志位被设置了，外部中断开启。尽管这个标志位能通过几种方法修改，由于JOS的简化，将只通过处理器保存和恢复%eflags寄存器来处理它，在我们进入和离开用户模式时。  
  
需要确保FL_IF标志位在用户环境运行时被设置，使得一个中断达到时，它能传递给处理器然后由中断代码处理。否则，中断被掩盖，或者忽视知道中断被重新开启。在启动加载器的最初的命令时掩盖了中断，然后至此都没有重新使能它们。  
  
> + Exercise13
    修改kern/trapentry.S和kern/trap.c来初始化正确的IDT记录并提供IRQ0-15的处理函数。然后修改kern/env.c的env_alloc(）函数来确保用户环境一直运行在中断使能状态。  
	同样要取消sched_halt()中的sti注释来让空转的CPU来解除中断掩盖。  
	处理器永远不会再申请一个硬件中断处理时压入错误码到栈中。可能需要重新阅读[80386参考指南](https://pdos.csail.mit.edu/6.828/2018/readings/i386/toc.htm)或者[IA-32 Intel架构软件开发者指南卷三](https://pdos.csail.mit.edu/6.828/2018/readings/ia32/IA32-3A.pdf)。  
	在这个练习后，如果运行内核来跑一个非短时的程序（如spin），会看到内核打印硬件中断的陷入帧。当中断在处理器里开启，JOS还不能处理它们，所以会误认为每个中断是向当前运行的用户环境的并销毁它。最终它应该会耗尽环境而掉入监控器。  
  
根据之前challenge的成果只需要在kern/trapentry.S里注册IRQ即可，trap_init会自动填充IDT。  
添加代码：  
```
TRAPHANDLER(irq0_entry, IRQ_OFFSET + 0, 0, 0);
TRAPHANDLER(irq1_entry, IRQ_OFFSET + 1, 0, 0);
TRAPHANDLER(irq2_entry, IRQ_OFFSET + 2, 0, 0);
TRAPHANDLER(irq3_entry, IRQ_OFFSET + 3, 0, 0);
TRAPHANDLER(irq4_entry, IRQ_OFFSET + 4, 0, 0);
TRAPHANDLER(irq5_entry, IRQ_OFFSET + 5, 0, 0);
TRAPHANDLER(irq6_entry, IRQ_OFFSET + 6, 0, 0);
TRAPHANDLER(irq7_entry, IRQ_OFFSET + 7, 0, 0);
TRAPHANDLER(irq8_entry, IRQ_OFFSET + 8, 0, 0);
TRAPHANDLER(irq9_entry, IRQ_OFFSET + 9, 0, 0);
TRAPHANDLER(irq10_entry, IRQ_OFFSET + 10, 0, 0);
TRAPHANDLER(irq11_entry, IRQ_OFFSET + 11, 0, 0);
TRAPHANDLER(irq12_entry, IRQ_OFFSET + 12, 0, 0);
TRAPHANDLER(irq13_entry, IRQ_OFFSET + 13, 0, 0);
TRAPHANDLER(irq14_entry, IRQ_OFFSET + 14, 0, 0);
TRAPHANDLER(irq15_entry, IRQ_OFFSET + 15, 0, 0);
```
env.c的env_alloc中开启中断：  
```
	e->env_tf.tf_eflags |= FL_IF;	

```
sched_halt()中sti取消注释。  

####处理时钟中断
  
在user/spin程序中，子程序开始运行后，它就悬停在一个循环里，内核永远都无法取回控制。需要编程硬件来定期地产生时钟中断，导致控制强制返回内核中，从而切换到不同的用户环境。  
  
lapic_init和pic_init (在init.c的i386_init中）调用设置了时钟和中断控制器来产生中断。需要写代码来控制这些中断。  
  
> + Exercise 14
	修改内核的trap_dispatch()函数使得它调用sched_yield() 来找到和运行一个不同的环境，当一个时钟中断发生时。
	现在可通过spin测试了：父环境fork出子环境，sys_yield（）切换到它数次但每次都能在一小段时间后重获CPU控制，最终结束子环境被优雅的终止。  
  
处理时钟中断，不要忘记在调用调度函数之前用lapic_eoi()确认获得中断。
```
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
                lapic_eoi();
                sched_yield();
                return;
 	}
```
  
  
这是做回归测试的好时机。确认没有因为开启中断破坏之前实验的成果（如forktree)。同样的尝试在多处理器下测试。现在应该能通过压力测试了，而make grade得分应该为65/80。  
  
运行spin，得到输出：  
```
[00000000] new env 00001000
I am the parent.  Forking the child...
[00001000] new env 00001001
I am the parent.  Running the child...
I am the child.  Spinning...
I am the parent.  Killing the child...
[00001000] destroying 00001001
[00001000] free env 00001001
[00001000] exiting gracefully
[00001000] free env 00001000
```
运行make grade,得分65/80。  
  
###进程间通信（IPC)
  
（准确的说对于JOS是环境间通信（IEC)但其他所有人都叫它IPC，所以这里按照大众的标准。）  
  
已经关注于操作系统各自独立的部分，这种方式产生了每个程序都有一个它自己机器的假象。另一个重要的操作系统服务是允许程序在它们想要时互相通信。让程序互动值一个相当强大的功能。Unix管道模型就是经典的例子。  
  
有很多的进程间通信模型。直到今日仍然有哪种模型最好的讨论。本实验不讨论，作为替代，将实现一个简单的IPC机制。  
  
####JOS的IPC
  
实现几个额外的JOS内核调用来共同地提供一个简单地进程间通信机制。实现两个系统调用 sys_ipc_recv 和sys_ipc_try_send。然后将实现两个库封装ipc_recv和ipc_send。  
  
使用JOS的IPC机制，用户环境可以相互发送的信息由两部分组成：一个32位值，和可选的单页映射。允许环境在消息中传递页映射提供了一个有效的方法来转换更多的数据，和放进32位整型相比，同样也可以允许进程设置共享内存安排更简单。  
  
####发送和接受消息
  
为了接受一个消息，一个环境会调用sys_ipc_recv。这个系统调用解除了当前环境的调度，知道一个消息被收到前都不会允许它。当一个环境等待接受消息，所有其他环境可以发送它一个消息-不只是一个特定环境，也不只是有着亲缘关系的环境间。换句话说，Part a里实现的许可检查不会应用到IPC上，因为IPC系统调用仔细设计过了是安全地：一个环境发送它的消息不会引起另一个环境故障（除非目标环境太忙了）。
  
为了尝试发送一个值，一个环境会调用sys_ipc_try_send，传递接受环境的id和发送值。如果命名过的环境正在接受（它已经调用了sys_ipc_recv而还没有获得值），那么这个发送传递消息并返回0。否则，发送返回 -E_IPC_NOT_RECV提示目标环境现在不期待接受一个值。  
  
用户空间库函数 ipc_recv负责调用ys_ipc_recv，然后在当前环境的Env结构中查找接受的值。  
  
相似的，库函数 ipc_send负责重复调用sys_ipc_try_send直到发送成功。  
  
####页传递
  
当环境用有效dsta参数（低于UTOP)调用sys_ipc_recv时，环境表达它愿意接受一个页映射。如果发送者发送了一个页，然后那个页应该被映射到接收者虚拟地址空间的dstva。如果接收者已经有了一个页在dstva，那么之前的页会解除映射。  
  
当一个环境用有效参数srcva(低于UTOP)来调用sys_ipc_try_send，它意味着发送者打算发送映射在srcva的页到接收者，许可为perm。在一次成功的IPC后，发送者保存它原先地址空间中secva的映射，但接受者在它的地址空间指定的dstva获得了一个同样物理页的映射。作为结果这个页成为了发送者和接收者共享的页。  
  
如果通信双方都没有指示一个页应该被传递，那么就没有页被传递。在任一IPC后内核设置接收者的Env结构中的新字段env_ipc_perm为接受页的许可，或者没收到页时为0。  
  
####实现IPC
  
> + Exercise 15
	实现 kern/syscall.c的sys_ipc_recv和sys_ipc_try_send。在实现各自之前阅读注释，因为它们需要共同工作。当在这些方法里调用envid2env时，需要设置checkperm标志位为0，意味着所有环境都允许发送IPC消息到任一其他环境，然后内核不做任何特别的许可检查而不是区分目标envid是否有效。  
	然后实现 lib/ipc.c的ipc_recv和ipc_send函数。  
	使用user/pingpong和user/primes函数来测试IPC机制。user/primes会为每个素数生成一个新环境直到JOS用尽了环境。读读user/primes.c会很有趣，记录了所有场面下fork和IPC如何运作的。  
  
sys_ipc_try_send(）尝试发送valu到目标envid。如果secva < UTOP，那么发送当前映射在srcva的页，使得接收者得到一个相同页的复制的映射。  
发送失败返回 -E_IPC_NOT_RECV，如果目标没被阻塞，等待一个IPC。  
发送也可能因为下列原因失败。  
否则，发送成功，目标的ipc字段被更新为：    
env_ipc_recving设置为0来阻塞未来的发送;  
env_ipc_from设置为发送者的envid;  
env_ipc_value设置为value参数;  
env_ipc_perm 设置为perm如果有页传递的话，否则为0。  
目标环境再次被标记为可运行，从暂停的sys_ipc_recv system call返回0。  
（提示：sys_ipc_recv真的返回了吗？）  
如果发送者想要发送一个页而接收者没有请求一个页，那么没有页映射被传递，但也不会产生错误。  
ipc只在没有错误的条件下成立。    
错误包括：  
-E_BAD_ENV 如果环境envid不存在（不需要检查许可）。  
-E_IPC_NOT_RECV 如果envid当前没有阻塞在sys_ipc_recv里。  
-E_INVAL 如果srcva < UTOP但srcva未分配页；如果srcva < UTOP而许可不正确（见sys_page_alloc）；如果srcva < UTOP但srcva没有映射在调用者的地址空间；如果perm & PTE_W，但srcva在当前环境地址是只读的。   
-E_NO_MEM 如果没有足够内存来映射srcva到envid的地址空间。  
```
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	int r;
	pte_t *pte;
	struct PageInfo *pp;
	struct Env *env;

	if ((r = envid2env(envid, &env, 0)) < 0)
		return -E_BAD_ENV;
	if (env->env_ipc_recving != true || env->env_ipc_from != 0)
		return -E_IPC_NOT_RECV;
	if (srcva < (void *)UTOP && PGOFF(srcva))
		return -E_INVAL;
	if (srcva < (void *)UTOP) {
		if ((perm & PTE_P) == 0 || (perm & PTE_U) == 0)
			return -E_INVAL;
		if ((perm & ~(PTE_P | PTE_U | PTE_W | PTE_AVAIL)) != 0)
			return -E_INVAL;
	}
	if (srcva < (void *)UTOP && (pp = page_lookup(curenv->env_pgdir, srcva, &pte)) == NULL)
		return -E_INVAL;
	if (srcva < (void *)UTOP && (perm & PTE_W) != 0 && (*pte & PTE_W) == 0)
		return -E_INVAL;
	if (srcva < (void *)UTOP && env->env_ipc_dstva != 0) {
		if ((r = page_insert(env->env_pgdir, pp, env->env_ipc_dstva, perm)) < 0)
			return -E_NO_MEM;
		env->env_ipc_perm = perm;
	}
	
	env->env_ipc_from = curenv->env_id;
	env->env_ipc_recving = false;
	env->env_ipc_value = value;
	env->env_status = ENV_RUNNABLE;
	env->env_tf.tf_regs.reg_eax = 0;
	return 0;
}
```
  
sys_ipc_recv阻塞直到一个值被读取。用Env结构的env_ipc_recving和env_ipc_dstva字段记录自己想要接受，标记自己为不可运行状态，并放弃CPU控制。  
如果dstva小于UTOP，那么表示愿意接受一页数据。dstva是发送页应该被映射的虚拟地址。  
这个函数只在错误时返回，但系统调用最终将在成功时返回0。  
错误包括   -E_INVAL 如果dstva< UTOP但dstva没有分配页时。
```
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
    if (dstva < (void *)UTOP && PGOFF(dstva))
        return -E_INVAL;
    curenv->env_ipc_recving = true;
    curenv->env_ipc_dstva = dstva;
    curenv->env_status = ENV_NOT_RUNNABLE;
    curenv->env_ipc_from = 0;
    sched_yield();
    return 0;	
}
```
  
syscall中添加case:  
```
		case (SYS_ipc_try_send):
			return sys_ipc_try_send(a1, a2, (void *)a3, a4);
		case (SYS_ipc_recv):
			return sys_ipc_recv((void *)a1);
```
  
ipc_recv() 通过IPC接受一个值并返回它。  
如果pg非空，那么所有发送者发来的页都被映射到那个地址。  
如果from_env_store非空，那么IPC发送者的envid到 * from_env_store。  
如果perm_store非空，那么存储IPC发送者的页许可到 * perm_store（当一个页成功传递那么就不会有这个如果）。  
如果系统调用失败，那么存储0到 * fromenv 和 * perm（在它们非空时）并返回错误。  
否则，返回发送者发来的值。  
提示：使用thisenv来发现值和谁发送了它。如果pg为空，传递sys_ipc_recv一个它能理解为无页的值。（零不是正确的值，因为那是一个完美的有效地址来映射页。)
```
int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
{
	int r;

	if (pg == NULL)
		r = sys_ipc_recv((void *)UTOP);
	else
		r = sys_ipc_recv(pg);
	if (from_env_store != NULL)
		*from_env_store = r < 0 ? 0 : thisenv->env_ipc_from;
	if (perm_store != NULL)
		*perm_store = r < 0 ? 0 : thisenv->env_ipc_perm;
       	if (r < 0)	
		return r;
	else
		return thisenv->env_ipc_value;
}
```
ipc_send()发送val（和perm许可的pg，在pg非空时）到toenv。这个函数保持尝试直到成功。  
他不会报出-E_IPC_NOT_RECV以外的错误。  
提示：使用sys_yield()来对CPU友好。如果pg为空，传递sys_ipc_try_send一个它能理解为无页的值。（零不是正确的值。）  
```
void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
	int r;
	void *dstpg;
	
	dstpg = pg != NULL ? pg : (void *)UTOP;
	while((r = sys_ipc_try_send(to_env, val, dstpg, perm)) < 0) {
		if (r != -E_IPC_NOT_RECV)
			panic("ipc_send: send message error %e", r);
		sys_yield();
	}
}
```  
   
运行pingpong，得到输出：  
```
[00000000] new env 00001000
[00001000] new env 00001001
send 0 from 1000 to 1001
1001 got 0 from 1000
1000 got 1 from 1001
1001 got 2 from 1000
1000 got 3 from 1001
1001 got 4 from 1000
1000 got 5 from 1001
1001 got 6 from 1000
1000 got 7 from 1001
1001 got 8 from 1000
1000 got 9 from 1001
[00001000] exiting gracefully
[00001000] free env 00001000
1001 got 10 from 1000
[00001001] exiting gracefully
[00001001] free env 00001001
```
运行primes,得到输出：  
```
CPU 0: 2 [00001001] new env 00001002
CPU 0: 3 [00001002] new env 00001003
CPU 0: 5 [00001003] new env 00001004
CPU 0: 7 [00001004] new env 00001005
CPU 0: 11 [00001005] new env 00001006
CPU 0: 13 [00001006] new env 00001007
CPU 0: 17 [00001007] new env 00001008
CPU 0: 19 [00001008] new env 00001009
CPU 0: 23 [00001009] new env 0000100a
CPU 0: 29 [0000100a] new env 0000100b
CPU 0: 31 [0000100b] new env 0000100c
CPU 0: 37 [0000100c] new env 0000100d
CPU 0: 41 [0000100d] new env 0000100e
......
````
  
至此，make grade 80/80，Lab4完成。