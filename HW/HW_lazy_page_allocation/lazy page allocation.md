# Homework lazy page allocation
  
堆内存的懒惰分配是o/s可以对页表硬件优化的一个小技巧。xv6应用用sbrk()向内核要求堆内存。目前使用的内核版本中，sbrk()分配物理内存并映射到进程的虚拟地址空间。有一些程序分配了内存却永远不使用它，例如为了实现大段稀疏的数组。先进的内核会推迟每个内存页的分配直到应用尝试使用那个页--直到收到一个页错误的信号。这个练习中要添加这一懒惰分配特性到xv6里。  
  
##Part a 从sbrk()中清除分配
  
第一个任务是删除sbrk(n)系统调用实现中的页分配，在sysproc.c中的sys_sbrk()函数中。sbrk(n)系统调用增大n个字节的进程的内存大小，返回新分配区域的起始位置（例如旧大小的结尾）。新sbrk(n)应该只增加n字节进程的大小（myproc()->sz)，并返回旧尺寸的区域。它不应当分配内存，删除对growproc()的调用（但任然需要增加进程的大小）。  
```
int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  // 未开启懒惰分配
  // if(growproc(n) < 0)
  //   return -1;
  myproc()->sz += n;
  return addr;
}
```
  
猜想这个修改的结果会导致什么？在哪里停下？  
 
  
修改后，启动xv6，输入echo hi到shell。可以得到如下输出：  
```  
init:starting sh
$ echo hi
pid 3 sh: trap 14 err 6 on cpu 0 eip 0x12f1 addr 0x4004--kill proc
$ 
```
这个消息来自trap.c中的内核陷入处理器；它捕获了一个页错误（trap 14或者说T_PGFLT），现在xv6内核不知道如何处理。确保理解为什么这个页错误会发生。“addr 0x4004”提示了导致这个页错误的虚拟地址是0x4004。  

在使用未分配内存地址时产生页错误。检查echo调用，在sh.c中调用了malloc时访问了未分配内存的地址。
  
##Part b 懒惰分配
  
修改trap.c中回应用户空间页错误的代码，映射一个新分配的物理内存页到发生错误的地址，然后返回到用户空间让进程继续执行。在产生错误信息的cprintf调用之前添加修改的代码。代码不要求覆盖所有的边界情况和错误环境；只要足够让sh运行像echo,ls这样的简单指令。  
  
提示：查看cprintf的参数来发现如何找到引起页错误的虚拟地址。 
rcr2()指向页错误地址。
 
提示：从vm.c的allocuvm()中抄写代码，它被sbrk()通过growproc（）调用。  
```
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  if(newsz >= KERNBASE)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}
```
  
提示：使用PGRUNDDOWN(va)来把错误虚拟地址对齐到页边界。
  
提示：break或return来避免cprintf和myproc()->killed =1。
  
提示：调用mappages()。因此删除vm.c中mappages声明的static修饰符，并在trap.c中声明它。在调用mappages（）前添加这个声明到trap.c中：  
`      int mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm);`  
  
提示：通过检查tf->trapno是否等于trap()里的T_PGFLT来确认一个错误是否是页错误。  

在trap中switch(tf->trapno)的default前添加case PGFLT，分配失败则在default中中断。  
```
  case T_PGFLT:
{
      char *mem;
      uint a;

      a = PGROUNDDOWN(rcr2());
      mem = kalloc();
      if (mem == 0) {
          cprintf("kalloc out of memory!\n");
          myproc()->killed = 1;
          break;
      }
      memset(mem, 0, PGSIZE);
      mappages(myproc()->pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U);
      break;
}
    
```

  
如果所有的工作都正确进行，懒惰分配代码会使echo i正常工作。shell里应该获得至少一个页错误（即懒惰分配），也可能是两个。  
  
顺便一提，这不是一个完整正确的实现。  
  
可选挑战：处理非法sbrk（）参数。处理类似于sbrk()参数太大的错误情况。证实在sbrk()分配过的地址没有内存分配时fork()和exit（）仍然可以正确工作。正确处理栈下的无效页。确认内核能正常使用尚未分配的用户地址--例如一个程序传递了一个sbrk()分配过的地址来调用read()。