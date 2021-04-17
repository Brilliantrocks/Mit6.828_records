#Homework User-level threads
  
本节的安排为通过执行线程间的文段切换实现一个简单的用户等级线程包。  
  
##切换线程  
  
下载[uthread.c](https://pdos.csail.mit.edu/6.828/2018/homework/uthread.c)和[uthread_switch.S]( https://pdos.csail.mit.edu/6.828/2018/homework/uthread_switch.S)到xv6目录下。确认uthread_switch.S后缀为.S，而不是.s。在xv6 Makefile里_forktest规则添加下列规则：  
```
_uthread: uthread.o uthread_switch.o
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o _uthread uthread.o uthread_switch.o $(ULIB)
	$(OBJDUMP) -S _uthread > uthread.asm
```
  
确保每行最开始为tab，而不是空格。  
  
添加_uthread到Makefile的用户程序列表UPROGS。  
  
运行xv6，然后从xv6 shell里运行uthread。xv6会打印出一个关于遭遇页错误的错误信息。    
  
完成thread_switch.S，使得输出类似如下（在单一CPU下）：  
   
```
~/classes/6828/xv6$ make CPUS=1 qemu-nox
dd if=/dev/zero of=xv6.img count=10000
10000+0 records in
10000+0 records out
5120000 bytes (5.1 MB, 4.9 MiB) copied, 0.0190287 s, 269 MB/s
dd if=bootblock of=xv6.img conv=notrunc
1+0 records in
1+0 records out
512 bytes copied, 7.2168e-05 s, 7.1 MB/s
dd if=kernel of=xv6.img seek=1 conv=notrunc
291+1 records in
291+1 records out
149040 bytes (149 kB, 146 KiB) copied, 0.000528827 s, 282 MB/s
qemu-system-i386 -nographic -drive file=fs.img,index=1,media=disk,format=raw -drive file=xv6.img,index=0,media=disk,format=raw -smp 1 -m 512 
xv6...
cpu0: starting
sb: size 1000 nblocks 941 ninodes 200 nlog 30 logstart 2 inodestart 32 bmap start 58
init: starting sh
$ uthread
my thread running
my thread 0x2D68
my thread running
my thread 0x4D70
my thread 0x2D68
my thread 0x4D70
my thread 0x2D68
my thread 0x4D70
my thread 0x2D68
...
````
uthread创建两个线程并在二者之间切出和切入。每个线程打印“my thread...”然后调度到另一个线程来运行。  
  
为了观测输出。需要完成thread_switch.S，但在跳入thread_switch.S前，首先要理解uthread.c如何使用thread_switch.S的两个全局变量current_thread和next_thread。这两个都是指向一个thread结构的指针。线程结构有一个线程栈和一个保存的栈指针（sp，指向线程栈）。uthread_switch用来保存当前线程状态到current_thread指向的结构，恢复到next_thread的状态，然后使得current_thread指向next_thread曾指向的位置，使得uthread_switch返回时next_thread在运行并成为新的current_thread。  
  
学习thread_create，为新线程设置初始栈。它提供了thread_switch需要做的。目的在于thread_switch 使用汇编指令popal和pushal来恢复和保存所有的x86寄存器。注意 thread_create模拟了八个压入的寄存器（32字节）到一个新线程栈。  
  
为了写thread_switch的汇编代码，需要知道C编译器如何分布struct thread到内存里：  
    --------------------
    | 4 bytes for state|
    --------------------
    | stack size bytes |
    | for stack        |
    --------------------
    | 4 bytes for sp   |
    --------------------  <--- current_thread
         ......

         ......
    --------------------
    | 4 bytes for state|
    --------------------
    | stack size bytes |
    | for stack        |
    --------------------
    | 4 bytes for sp   |
    --------------------  <--- next_thread  
  
变量next_thread和current_thread都包含了一个thread结构的地址。  
  
为了完成current_thread的sp字段，写如下的汇编代码：  
	movl current_thread, %eax   
	movl %esp, (%eax)   
  
指令将%esp保存在 current_thread_>sp里,因为sp在结构里的偏移量为0。可以从uthread.asm里学习编译器为uthread.c生成的汇编码。    
  
thread_switch.S实现：  
```
	.globl thread_switch
thread_switch:
	pushal/* 保存寄存器状态 */
	movl current_thread, %eax
	movl %esp, (%eax)/* 令curent_thread->指向 当前esp */
	
	movl next_thread, %ebx
	movl %ebx, current_thread/* 令next_thread 成为新的current_thread */
	movl (%ebx), %esp/* 将当前esp指向 next_thread */
	popal/* 恢复寄存器状态 */
	
	movl $0x0, next_thread	/* 清除 next_thread */**
	ret				/* pop return address from stack */
```
启动模拟器测试，得到输出：  
  
```
$ uthread
my thread running
my thread 0x2DC8
my thread running
my thread 0x4DD0
my thread 0x2DC8
my thread 0x4DD0
my thread 0x2DC8
my thread 0x4DD0
my thread 0x2DC8
my thread 0x4DD0
my thread 0x2DC8
my thread 0x4DD0
my thread 0x2DC8
my thread 0x4DD0
....
```
  
用gdb单步调试来测试thread_switch会很用。如下：  
```
(gdb) symbol-file _uthread
Load new symbol table from "/Users/kaashoek/classes/6828/xv6/_uthread"? (y or n) y
Reading symbols from /Users/kaashoek/classes/6828/xv6/_uthread...done.
(gdb) b thread_switch
Breakpoint 1 at 0x204: file uthread_switch.S, line 9.
(gdb) 
```
  
断点也许（或不）在运行线程之前就触发了，为什么？  
  
一旦xv6 shell运行，输入uthread，然后gdb会断在thread_switch。现在可以输入下列指令来观察线程状态：  
```
(gdb) p/x next_thread->sp
$4 = 0x4ae8
(gdb) x/9x next_thread->sp
0x4ae8 :      0x00000000      0x00000000      0x00000000      0x00000000
0x4af8 :      0x00000000      0x00000000      0x00000000      0x00000000
0x4b08 :      0x000000d8
```  
  
至此，本节完成