#Homework: locking
  
本节将探索中断和锁的交互。  
  
##不要这么做
  
确认理解如果xv6内核执行下面的代码会发生什么：  
```
  struct spinlock lk;
  initlock(&lk, "test lock");
  acquire(&lk);
  acquire(&lk);
```
（acquire在spinlock.c中。）    
  
解释发生了什么。  
  
在第一条acquire请求锁后紧接着再次发起了acquire请求，在acquire的实现中当重复请求锁时会报错。  
  
##ide.c中的中断
  
一个acquire确保中断在局部处理器通过pushcli()用cli指令时被停用，并让中断保持关闭直到那个处理器释放最后一个持有的锁（用sti指令启用）。  
  
现在看看如果在持有ide锁时启用中断会发生什么。在ide.c的idew中，在acquire()后添加一个sti()调用，然后再release()前调用cli()。重启内核并在QEMU中启动。内核可能会在启动后不久报错；如果没有的活再启动QEMU几次。  
  
解释为什么内核报错。kernel.asm列出的栈追踪（panic打印的%eip的值）会很有用。  
  
错误码  
```
lapicid 1: panic: acquire
 80104411 80102093 80105965 801056ec 80100183 80101455 801014cf 80103704 801056ef 0
```
kernel.asm的记录  
```
80104411:	eb 0d                	jmp    80104420 <release>
80102093:	8b 1d 64 a5 10 80    	mov    0x8010a564,%ebx
80105965:	e9 9d fe ff ff       	jmp    80105807 <trap+0x57>
801056ec:	83 c4 04             	add    $0x4,%esp
80100183:	83 c4 10             	add    $0x10,%esp
80101455:	89 c3                	mov    %eax,%ebx
801014cf:	ff 35 d8 09 11 80    	pushl  0x801109d8
80103704:	c7 04 24 01 00 00 00 	movl   $0x1,(%esp)
801056ef <trapret>:
```
在acquire后启用中断，程序接受了中断并进入ideintr()中，在这个函数里又一次调用acquire请求锁，故报错。  
  
  
###file.c中的中断  
  
移除上面的测试用sti()和cli()，重新build内核，确认它能正确工作。  
  
现在看看当持有file_table_lock时启动中断会发生什么。这个锁保护了文件描述符的表，此表在应用打开或关闭文件被内核修改。在file.c的filealloc()中，于acquire（）后添加一个sti()的调用，并在每个release（）前添加cli()。同时包含文件`#include "x86.h"`到文件顶部在其他包含行后。重构内核并在qemu中启动。这应该会报错。  
  
解释为何内核不会报错。为什么file_table_lock和ide_lock在这方面有着不同的表现?  
  
因为申请file_table_lock锁的函数有filealloc(),filedup()和fileclose()函数，后两者并没有在启动时调用，故不会出现重复请求锁的情况，所以没有报错。而ide_lock开启中断后的中断处理函数中再次请求了锁，会导致报错。
  
回答这个问题不需要理解IDE硬件的太多细节，但看看每个acquire锁定了什么函数，它们什么时候被调用会很有用。  
  
在qemu上有一种小小的可能，内核会在filealloc()中有额外sti()时报错。如果内核不报错，检查释放移除了idew中的sti()。如果它继续报错而唯一的sti()在filealloc()中，那么想想为何与真实的硬件不同。  
  
为什么release（）清除了lk->pcs[0]和lk=>cpu在清除lk->locked之前？为什么不能在之后呢？
  
因为先清除锁标记的话可能会有别的线程修改了锁的信息，本线程再清除后会导致锁信息丢失产生bug。  
  
至此本节完成。