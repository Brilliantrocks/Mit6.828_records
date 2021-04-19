# Homework: boot xv6
## 启动xv6
获取xv-6。
`$ git clone git://github.com/mit-pdos/xv6-public.git`  
`cd xv6-public`    
`make`  
  
## 找到并设置地址断点  
找到内核进入指针_start:
` nm kernel | grep _start`  
得到输出  
```
8010a48c D _binary_entryother_start
8010a460 D _binary_initcode_start
0010000c T _start
```
 
这里所需地址为 0010000c  
  
运行内核自带的QEMU GDB，在_start处设置断点
`$ make qemu-gdb`  
gdb初始化，Makefile中写入
```
gdb:
	gdb -x .gdbinit
```
运行
`$ make gdb`
在新的终端里到同样的目录下输入指令：
`$ gdb`  
`(gdb) br * 0010000c`  
`(gdb) c`  
得到输出  
```
Continuing.
The target architecture is assumed to be i386
=> 0x10000c:	mov    %cr4,%eax 
Breakpoint 1, 0x0010000c in ?? ()
```
虽然因为gdb版本的不同在细节上有差异，但都会在同一个断点处停下，位于mov指令之上。  
  
## Exercise:栈里有什么？  
当停在这个断点之上时，查看寄存器和栈的内容：  
```
(gdb) info reg
...
(gdb) x/24x $esp
...
(gdb)
```  
得到输出  
```
(gdb) info reg
eax            0x0	0
ecx            0x0	0
edx            0x1f0	496
ebx            0x10094	65684
esp            0x7bdc	0x7bdc
ebp            0x7bf8	0x7bf8
esi            0x10094	65684
edi            0x0	0
eip            0x10000c	0x10000c
eflags         0x46	[ PF ZF ]
cs             0x8	8
ss             0x10	16
ds             0x10	16
es             0x10	16
fs             0x0	0
gs             0x0	0
(gdb) x/24x $esp
0x7bdc:	0x00007d8d	0x00000000	0x00000000	0x00000000
0x7bec:	0x00000000	0x00000000	0x00000000	0x00000000
0x7bfc:	0x00007c4d	0x8ec031fa	0x8ec08ed8	0xa864e4d0
0x7c0c:	0xb0fa7502	0xe464e6d1	0x7502a864	0xe6dfb0fa
0x7c1c:	0x16010f60	0x200f7c78	0xc88366c0	0xc0220f01
0x7c2c:	0x087c31ea	0x10b86600	0x8ed88e00	0x66d08ec0
```
对栈中每个非零值写三五个词的注释解释它是什么，打印的结果中实际哪些部分才是栈？  
查阅bootasm.S,bootmain.c和bootlock.asm（包含了编译器/汇编器的输出）会很有用。参考书籍有关于x86汇编文档的部分，如果好奇某些具体指令的语义是什么意思的话可以参考。目标是理解和解释所见栈的内容，在刚刚进入x86内核时。观察栈在早期启动阶段在哪里如何设置的，追踪栈的改变直到有兴趣的断点处。下面的问题会帮助理解：  
  
> + 从重启qemu和gdb开始，设置断点到 0x7c00，bootasm.S中启动区块的开始处。单步调试后续指令（si)。在bootams.S的哪里栈指针被初始化？（si直到看见一个赋值给%esp的指令，即栈指针的寄存器）。
> + si通过bootmain的调用，现在栈里有什么？
> + bootmain的第一条汇编指令对栈做了什么？在bootblock.asm中寻找。
> + 继续通过gdb追踪，寻找改变eip到0x1000c的调用。这个调用对栈做了什么？（提示：想想这个调用尝试做什么，在bootmain.c中证实这一点，找到bootbloack.asm中对应的bootmain代码指令。这会帮助你设置合适的断点。）
  

1.`=> 0x7c43:	mov    $0x7c00,%esp`处启用了栈指针。  
2.下一条指令`=> 0x7c48:	call   0x7d3b`调用了bootmain,执行后esp存入0x00007c4d，为bootmain的调用返回地址。
3.调用后第一个指令为`=> 0x7d3b:	push   %ebp`，将原ebp的值入栈保存。
4.在entry()中执行`=> 0x7d87:	call   *0x10018`后，跳到了0x1000c处。这个指令进入ELF头，即内核部分。检查esp的值，该调用存入了0x00007d8d，即调用返回地址。 
  
分析栈中数据：
栈底为0x00007c00,此地址向下为栈。  
0x7bdc: 0x00007d8d 为调用内核的返回地址  
0x7bfc: 0x00007c4d 调用bootmain的返回地址  
0x7cc0: 0x8ec031fa cli指令
