# Lab1  
## Part a. PC Bootstrap 计算机启动引导程序  
这一部分实验不需要写代码，	而要求熟悉x86汇编。  
课程推荐了一系列整个6.828会涉及到的[读物](https://pdos.csail.mit.edu/6.828/2018/reference.html)作为参考，在Lab1里还为不熟悉汇编语言的人推荐了[PC Assembly Language Book](https://pdos.csail.mit.edu/6.828/2018/readings/pcasm-book.pdf)。值得一提的是，这本指导书中的例子是以Intel语法风格为NASM汇编器写的，而我们用到的GNU汇编器用到的代码是以AT&T语法风格写的。两种语法风格的简单转换参见[Brennan's Guide to Inline Assembly](http://www.delorie.com/djgpp/doc/brennan/brennan_att_inline_djgpp.html)。  
#### Exercise 1 
熟悉参考读物里关于汇编语言的部分，推荐搭配阅读两种语法风格转换的文章。  
### x86系统模拟
JOS下QEMU的启动参照Tools.md部分。  
```
Booting from Hard Disk...
6828 decimal is XXX octal!
entering test_backtrace 5
entering test_backtrace 4
entering test_backtrace 3
entering test_backtrace 2
entering test_backtrace 1
entering test_backtrace 0
leaving test_backtrace 0
leaving test_backtrace 1
leaving test_backtrace 2
leaving test_backtrace 3
leaving test_backtrace 4
leaving test_backtrace 5
Welcome to the JOS kernel monitor!
Type 'help' for a list of commands.
K>
```  

从硬盘启动后的所有内容都会被JOS内核打印出来，以>k为命令提示符。  
这里只有两个命令可以给内核监测器：help和kerninfo。  
```
K> help
help - display this list of commands
kerninfo - display information about the kernel
K> kerninfo
Special kernel symbols:
  entry  f010000c (virt)  0010000c (phys)
  etext  f0101a75 (virt)  00101a75 (phys)
  edata  f0112300 (virt)  00112300 (phys)
  end    f0112960 (virt)  00112960 (phys)
Kernel executable memory footprint: 75KB
K>
```
### 计算机物理地址空间
一个计算机固有物理地址空间结构  

```
+------------------+  <- 0xFFFFFFFF (4GB)
|      32-bit      |
|  memory mapped   |
|     devices      |
|                  |
/\/\/\/\/\/\/\/\/\/\

/\/\/\/\/\/\/\/\/\/\
|                  |
|      Unused      |
|                  |
+------------------+  <- depends on amount of RAM
|                  |
|                  |
| Extended Memory  |
|                  |
|                  |
+------------------+  <- 0x00100000 (1MB)
|     BIOS ROM     |
+------------------+  <- 0x000F0000 (960KB)
|  16-bit devices, |
|  expansion ROMs  |
+------------------+  <- 0x000C0000 (768KB)
|   VGA Display    |
+------------------+  <- 0x000A0000 (640KB)
|                  |
|    Low Memory    |
|                  |
+------------------+  <- 0x00000000
```  
第一批基于16位Intel 8088处理器的计算机只能够寻址1MB的物理内存。这些早期计算机的的物理地址空间从0x00000000开始到0x000FFFFF结束，而不是结束于0xFFFFFFFF。640KB的Low Memory是早期计算机唯一能够使用的随机访问内存(RAM)。事实上最早期的计算机只能被设置16KB,32KB,64KB的RAM。  
  
从0x000A0000到0x000FFFFF的区域是硬件为特殊用处保留的，如视频显示缓冲区和固件支持非易失存储器之类。
这一段保留区域中最重要的部分是占据了从0x00F0000到0x000FFFFF之间64KB空间的基础输入输出系统（BIOS）。早期计算机里BIOS被放在真实只读内存ROM中，如今计算机把BIOS存储在可更新闪存中。BIOS负责运转基础系统初始化如激活显卡和检查已安装的内存数量。初始化后，BIOS从一些合适的地方加载操作系统，如软盘，硬盘，CD-ROM，或者网络，然后转交机器控制权给操作系统。  
  
在Intel用80286和80386处理器终于突破了“一兆字节障碍”后，尽管已经支持了16MB/4GB的物理地址空间，计算机设计人员仍然保留了原先的1MB物理内存布局，以便兼容已有的软件。因此，现代计算机在0x000A0000到0x00100000的物理地址上有一段“空洞”将RAM划分成最初64KB的“低位”，“传统”内存和剩余的扩展内存。另外，一些位于计算机32位物理地址空间最顶部，在所有物理RAM之上的区域，如今通常被BIOS保留，供32位总线设备的使用。  
  
最近的x86处理器能够支持多于4GB的物理RAM,所以RAM地址能够拓展到远远超过0XFFFFFFFF。在这种情况下BIOS一定要在系统RAM32位可寻址区域的顶部安排留下第二个空洞空间，以便让那些32位设备来映射。由于设计上的限制，JOS将只用到计算机物理内存最初的256MB，所以我们将假装所有的计算机只有32位物理地址空间。事实上，处理复杂物理地址空间与其他发展数年的硬件设计领域是OS发展的一个重大实践挑战。  
  
### THE ROM BIOS
在实验的这一部分，要求使用QEMU的debug功能调查intel32位兼容计算机是如何启动的。  
  
建立两个窗口，都cd转到lab目录，其中一个启动QEMU，SSH远程登陆操作推荐使用`make qemu-nox-gdb`指令启动。此时QEMU在处理器执行第一个命令前停下了，等待一个来自gdb的debug连接。在第二个窗口中输入`make gdb`，得到类似如下的输出  
```
ubuntu@VM-0-3-ubuntu:/usr/local/6.828/lab$ make gdb
gdb -n -x .gdbinit
GNU gdb (Ubuntu 8.1.1-0ubuntu1) 8.1.1
Copyright (C) 2018 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
and "show warranty" for details.
This GDB was configured as "x86_64-linux-gnu".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<http://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
<http://www.gnu.org/software/gdb/documentation/>.
For help, type "help".
---Type <return> to continue, or q <return> to quit--- #回车
Type "apropos word" to search for commands related to "word".
+ target remote localhost:25000
warning: No executable has been specified and target does not support
determining executable automatically.  Try using the "file" command.
warning: A handler for the OS ABI "GNU/Linux" is not built into this configuration
of GDB.  Attempting to continue with the default i8086 settings.

The target architecture is assumed to be i8086
[f000:fff0]    0xffff0:	ljmp   $0xf000,$0xe05b
0x0000fff0 in ?? ()
+ symbol-file obj/kern/kernel
(gdb)
```  
课程提供了一个.gdbinit文件启动GDB来对早期启动的16位代码进行debug，然后导向连接到监听中的QEMU。  
  
下一行：
`[f000:fff0]    0xffff0:	ljmp   $0xf000,$0xe05b`  
是GDB对第一个将要执行的指令的反汇编。从这个输出我们得知：  
> + IBM架构计算机从物理地址的0x000ffff0，刚好是最顶端64KB，为ROM BIOS保留的区域。  
> + 计算机设置代码段寄存器CS为0xf000，中断优先级寄存器IP为0xfff0。
> + 第一个将被执行的命令是一个jmp指令，用于跳转到分段地址cs = 0xf000和ip = 0xe05b  
  
这是Intel如何设计8088处理器的，也是IBM用在他们的原型计算机上的设计。  
计算机里的BIOS硬连在0x000f0000-0x000fffff的地址上的设计，确保了在通电或是系统重启后BIOS总能第一个获得机器的控制权。这是至关重要的，因为在通电时机器RAM里其他地方没有处理器能够执行的软件程序。QEMU模拟器有它自己的BIOS，放在处理器的模拟物理地址空间中。一旦处理器重置，这个模拟处理器进入实模式，设置CS到0xf000，IP到0xfff0，所以这个执行从CS:IP段地址开始。  
  
关于分段地址0xf000:fff0转化为物理地址的方法，我们需要了解实模式寻址方式。在实模式（即计算机启动时的模式）中，地址以公式 `物理地址=16*段基址+段偏移`转换。  
```
16 * 0xf000 + 0xfff0
= 0xf0000 + 0xfff0
= 0xffff0
```
0xffff0是BIOS末尾（0x100000）之前的16字节。因此我们不该意外BIOS做的第一件事是向后jmp到一个BIOS中的更早的位置来获得更大的空间。  
  
#### Exercise 2
使用GDB的si单步调试命令跟踪ROM BIOS的一些指令，推测这些指令在做什么。推荐查阅[Phil Storrs I/O Ports Description](http://web.archive.org/web/20040404164813/members.iweb.net.au/~pstorr/pcbook/book2/book2.htm)。不需要完全查清细节，只要大体明白BIOS一开始在做什么。  
  
  
BIOS启动后首先会检查物理设备，即Power-On Self-Test，此时只对CPU各项寄存器检查，判断是否满足计算机运行的基本条件，如果此阶段出现故障一般会通过主板蜂鸣器发出规律的报警声。无意外发生时之后BIOS会对所有计算机硬件初始化，为操作系统管理和硬件调配做准备。BIOS第一个初始化的硬件是显卡，将后续初始化硬件信息打印在显示器上，如果有，提示硬件初始化时可能发生的问题。  
  
接下来会检查8254 Timer（可编程计数器），8259A（可编程中断控制器），8237DMA Controller(DMA 控制器)的状态。  
  
Initial――针对动态内存(DRAM)，主板芯片组，显卡以及相关外围的寄存器做初始化设置，并检测是否能够工作。
所谓初始化设置，就是依照该芯片组的技术文件检定，做一些寄存器的填值，改位的动作，使得主板/芯片组和内存，I/O的功能得以正常运行。  
  
记录系统的设置值，并且存储在非挥发性内存（Non-Volatile RAM），像CMOS或Flash Memory(ESCD区域)等。  
  
将常驻程序库(Runtime Program)常驻于某一段内存内，提供给操作系统或应用程序调用。  
  
BIOS自检过程后，如果没有错误，接下来就会进行操作系统引导。BIOS会根据CMOS里记录的启动顺序一个个地来尝试加载启动代码。  
  
GDB追踪：  
 
`[f000:e05b]    0xfe05b:	cmpl   $0x0,%cs:0x6ac8`  
CMP指令，比较0x0和%cs:0x6ac8地址的值。  
`[f000:e062]    0xfe062:	jne    0xfd2e1`  
jne指令，如果ZF标志位为0的时候跳转，即上一条指令cmpl的结果不是0时跳转，也就是$cs:0x6ac8地址处的值不是0x0时跳转。  
`[f000:e066]    0xfe066:	xor    %dx,%dx`  
执行位置后移一个字节，表示没有跳转。xor指令，把dx寄存器清零。  
`[f000:e068]    0xfe068:	mov    %dx,%ss`  
mov指令，将ss寄存器的值赋给dx寄存器。  
`[f000:e06a]    0xfe06a:	mov    $0x7000,%esp`  
`[f000:e070]    0xfe070:	mov    $0xf34c2,%edx`  
`[f000:e076]    0xfe076:	jmp    0xfd15c`  
`[f000:d15c]    0xfd15c:	mov    %eax,%ecx`  
一些列寄存器赋值改位，使得主板/芯片组和内存的IO功能正常运行。
`[f000:d15f]    0xfd15f:	cli`  
关闭中断指令，用于保护启动操作不被中断，如大部分硬件中断。  
`[f000:d160]    0xfd160:	cld`  
设置方向标识位为0，表示后续的串操作比如MOVS操作，内存地址的变化方向，如果为0代表从低地址值变为高地址。  
`[f000:d161]    0xfd161:	mov    $0x8f,%eax`  
`[f000:d167]    0xfd167:	out    %al,$0x70`  
`[f000:d169]    0xfd169:	in     $0x71,%al`  
IO端口控制CMOS关闭NMI不可屏蔽中断。  
`[f000:d16b]    0xfd16b:	in     $0x92,%al`  
`[f000:d16d]    0xfd16d:	or     $0x2,%al`  
`[f000:d16f]    0xfd16f:	out    %al,$0x92`  
IO端口控制PS/2系统控制，使能A20第二十一个总线地址，测试内存可用空间。  
`[f000:d171]    0xfd171:	lidtw  %cs:0x6ab8`  
lidt指令：加载中断向量表寄存器(IDTR)  
`[f000:d177]    0xfd177:	lgdtw  %cs:0x6a74`  
把从0xf6a74为起始地址处的6个字节的值加载到全局描述符表格寄存器中GDTR中。  
`[f000:d17d]    0xfd17d:	mov    %cr0,%eax`  
`[f000:d180]    0xfd180:	or     $0x1,%eax`  
`[f000:d184]    0xfd184:	mov    %eax,%cr0`  
CR0的PE位置1。  
`[f000:d187]    0xfd187:	ljmpl  $0x8,$0xfd18f` 
进入保护模式，可以解析4G内存。 
`=> 0xfd18f:	mov    $0x10,%eax`  
`=> 0xfd194:	mov    %eax,%ds`  
`=> 0xfd196:	mov    %eax,%es`  
`=> 0xfd198:	mov    %eax,%ss`  
`=> 0xfd19a:	mov    %eax,%fs`  
`=> 0xfd19c:	mov    %eax,%gs` 
设置剩余的段寄存器以适应保护模式。   
`=> 0xfd19e:	mov    %ecx,%eax`  
`=> 0xfd1a0:	jmp    *%edx`  
`=> 0xf34c2:	push   %ebx`  
`=> 0xf34c3:	sub    $0x2c,%esp`  
`=> 0xf34c6:	movl   $0xf5b5c,0x4(%esp)`  
`=> 0xf34ce:	movl   $0xf447b,(%esp)`  
`.......`  

