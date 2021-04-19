# Homework XV6 system call
## Part a:系统调用追踪 
  
第一个任务是修改xv6内核来为每个系统调用请求打印一行信息。一行足够打印系统调用名称和返回值；不需要打印系统调用参数。  
  
当完成时，在启动xv6时会有如下的输出：  
 
```
...
fork -> 2
exec -> 0
open -> 3
close -> 0
$write -> 1
 write -> 1
```
  
这是初始化fork和sh执行，确保只有两个文件描述符打开的sh，和写$提示符的sh。（注意：shell的输出和系统调用时互相混合的，因为shell使用了xie系统调用来打印它的输出）  
  
提示：修改syscall.c中的syscall()函数。  
  
查阅syscall.h中的定义，建立名称数组  
```
static char SYS_call_names[][6] = {
        [SYS_fork] "fork",
        [SYS_exit] "exit",
        [SYS_wait] "wait",
        [SYS_pipe] "pipe",
        [SYS_read] "read",
        [SYS_kill] "kill",
        [SYS_exec] "exec",
        [SYS_fstat] "fstat",
        [SYS_chdir] "chdir",
        [SYS_dup] "dup",
        [SYS_getpid] "getpid",
        [SYS_sbrk] "sbrk",
        [SYS_sleep] "sleep",
        [SYS_uptime] "uptime",
        [SYS_open] "open",
        [SYS_write] "write",
        [SYS_mknod] "mknod",
        [SYS_unlink] "unlink",
        [SYS_link] "link",
        [SYS_mkdir] "mkdir",
        [SYS_close] "close"
};

```
在系统调用中打印信息  
```
void
syscall(void)
{
  int num;
  struct proc *curproc = myproc();

  num = curproc->tf->eax;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    curproc->tf->eax = syscalls[num]();
    cprintf("\t%s\t->\t%d\n", SYS_call_names[num], num);//打印系统调用信息
  } else {
    cprintf("%d %s: unknown sys call %d\n",
            curproc->pid, curproc->name, num);
    curproc->tf->eax = -1;
  }
}
```
启动qemu得到输出：  
```
	exec	->	7
	open	->	15
	dup	->	10
	dup	->	10
i	write	->	16
n	write	->	16
i	write	->	16
t	write	->	16
:	write	->	16
 	write	->	16
s	write	->	16
t	write	->	16
a	write	->	16
r	write	->	16
t	write	->	16
i	write	->	16
n	write	->	16
g	write	->	16
 	write	->	16
s	write	->	16
h	write	->	16

	write	->	16
	fork	->	1
	exec	->	7
	open	->	15
	close	->	21
$	write	->	16
 	write	->	16

```  
  
## Part b:日期系统调用
  
第二个任务时增加一个系统调用到xv6中。这个练习主要是观察系统调用机制的即几个不同的地方。新系统调用会获得当前UTC时间并返回它到用户程序。使用工具函数cmostime()（定义在lap.c)来读取真实时间钟。date.h包含了struct rtcdate的定义，它被作为一个指针参数传给cmostime()。    
  
创建一个用户等级程序来调用新写的日期系统调用；在date.c中写入：  
```
#include "types.h"
#include "user.h"
#include "date.h"

int
main(int argc, char *argv[])
{
  struct rtcdate r;

  if (date(&r)) {
    printf(2, "date failed\n");
    exit();
  }

  // your code to print the time in any format you like...
printf(1, "%d-%d %d %d:%d:%d\n", r.month, r.day, r.year, r.hour, r.minute, r.second);
  exit();
}
````
为了使得新date程序能从xv6中运行，增加_date到Makefile的UPROGS定义里。  
  
制作一个日期系统调用的策略应该是克隆所有已存在的特定系统调用的代码段，例如“uptime"系统调用。在所有的源文件里grep寻找uptime，指令`grep -n uptime *.[chS]`。  
得到输出   
```
syscall.c:105:extern int sys_uptime(void);
syscall.c:121:[SYS_uptime]  sys_uptime,
syscall.c:144:        [SYS_uptime] "uptime",
syscall.h:15:#define SYS_uptime 14
sysproc.c:83:sys_uptime(void)
user.h:25:int uptime(void);
usys.S:31:SYSCALL(uptime)

```
参照添加date项  
```
syscall.c:106:extern int sys_date(void);
syscall.c:130:[SYS_date]  sys_date,
syscall.c:154:        [SYS_date] "date"
syscall.h:23:#define SYS_date   22
sysproc.c:93:sys_date(void)
user.h:26:int date(void);
usys.S:32:SYSCALL(date)
```
sysproc.c中sys_date的实现  
```
int
sys_date(void)
{
  struct rtcdate *rdate;
  if(argptr(0,(void *)&rdate, sizeof(*rdate)) < 0)
  return -1;
  cmostime(rdate);
  return 0;
}
```

  
完成时，输入date到一个xv6shell提示符会返回当前的UTC时间。
```
$ date
4-12 2021 19:13:34
```

  

