# Homework: CPU alarm
  
这个练习里需要向xv6添加一个在进程使用CPU时间定期警报的功能。这对想要限制能使用多少CPU时间的计算量限定进程或是在计算的同时采取某些定期行为的进程很有用。更通俗的说，实现一个原始的用户等级中断/错误处理器；例如，你可能会用到相似的东西来处理应用中的页错误。  
  
添加一个新的alarm(interval,handler)系统调用。如果一个应用调用了alarm(n,fn),那么在这个程序每n跳CPU时间后，内核会使得应用函数fn被调用。当fn返回时，应用会恢复到它离开的地方。一跳是一个xv6里使用的时间单位，由硬件计时器产生中断的频率决定。  
  
将下列样例程序放到alarmtest.c中  
```
#include "types.h"
#include "stat.h"
#include "user.h"

void periodic();

int
main(int argc, char *argv[])
{
  int i;
  printf(1, "alarmtest starting\n");
  alarm(10, periodic);
  for(i = 0; i < 25*500000; i++){
    if((i % 250000) == 0)
      write(2, ".", 1);
  }
  exit();
}

void
periodic()
{
  printf(1, "alarm!\n");
}
```
这个程序调用了alarm（10，periodic)来要求内核强制每10跳调用一次periodic()，然后悬停一小会。在实现了内核的alarm（）系统调用后，alarmtest会产生类似如下的输出：  
````
 $ alarmtest
alarmtest starting
.....alarm!
....alarm!
.....alarm!
......alarm!
.....alarm!
....alarm!
....alarm!
......alarm!
.....alarm!
...alarm!
...$ 
````
如果只看到了一个alarm！,试着增加alarmtest.c中的迭代器数目到10倍。  
  
提示:需要修改Make file来使得alarmtest.c在xv6用户程序里被编译。  
  
提示：user.h里放的正确声明为`int alarm(int ticks, void (*handler)());`。  
  
提示：同样要更新sysycall.h和usys.S来允许alarmtest来申请警报系统调用。  
  
提示：sys_alarm()应该存储警报间隔和处理函数指针到proc的新字段里；详见proc.h。  
  
提示：这里有一种sys_alarm()供参考：  
```
    int
    sys_alarm(void)
    {
      int ticks;
      void (*handler)();

      if(argint(0, &ticks) < 0)
        return -1;
      if(argptr(1, (char**)&handler, 1) < 0)
        return -1;
      myproc()->alarmticks = ticks;
      myproc()->alarmhandler = handler;
      return 0;
    }
```
把它添加到syscall.c中并添加SYS_ALARM记录到syscalls数组里。  
  
提示：需要保持追踪距离上次调用进程的警报处理器后经过了多少跳（或者直到下次调用时离开）；这里也要使用struct proc中的新字段。可以通过proc.c的allocproc()来初始化proc。    
  
提示：每经过一跳，硬件时钟会强制一次中断，在trap()的case T_IRQ0 + IRQ_TIMER中处理；在这里添加代码。    
  
提示：只在进程运行或者来自用户空间的计算器中断时操作一个进程的警报跳数；使用下面的语句  
`if(myproc() != 0 && (tf->cs & 3) == 3) ...`  
  
提示：在ITQ_TIMER代码中，当一个进程警报间隔到了，使其执行它的处理函数。  
  
提示：使得处理函数返回时，进程恢复到它离开的地方继续执行。  
  
提示：可以在alarmtest.asm中看到alarmtest程序的汇编码。  
  
提示：告诉QEMU只使用一个CPU会让查看陷入更简单，输入`make CPU=1 qemu` 。  
  

在Makefile的UPROG中添加_alarmtest
 
添加SYSCALL系统调用  
user.h中声明`int alarm(int ticks, void(*hander)());`    
usys.S中添加`SYSCALL(alarm)`  
syscall.h中添加`#define SYS_alarm  23`  
syscall.c中添加外部申明和和在syscall[]数组中注册  
`extern int sys_alarm(void);`  
`static int (*syscalls[])(void) = {...,[SYS_alarm]  sys_alarm,}`  
在proc.h的struct proc中追加新字段  
```
struct proc{
...
  int alarmticks;              // 警报间隔跳数
  int curalarmticks;           // 当前警报跳数
  void (*alarmhandler)();       // 警报处理函数
}
```
用上述函数在sysproc.c中定义sys_alarm（）  
  
修改trap（）中case T_IRQ0 + IRQ_TIMER:  
```
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    //判断用户进程并增加警报跳数
    if(myproc() != 0 && (tf->cs & 3) == 3)
    {
      myproc()->curalarmticks++;
      if(myproc()->alarmticks == myproc()->curalarmticks)
      {
        myproc()->curalarmticks = 0;
        //当前进程返回地址eip压栈保存
        tf->esp -= 4;    
        *((uint *)(tf->esp)) = tf->eip;
        //传递警报处理函数
        tf->eip =(uint) myproc()->alarmhandler;
      }
    }
    lapiceoi();
    break;
```
至此本节完成
