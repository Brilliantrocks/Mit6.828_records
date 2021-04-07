# Lab1
## Part b.The Boot Loader  启动加载器   
  
 
计算机用到软盘和硬盘被划分成若干个512字节的区域，叫sectorp扇区。一个扇区是硬/软盘的最小调度粒度。这导致每次读写操作都要有一个或若干个扇区大小，并且还得设置扇区边界。如果软/硬盘是可启动的，则它的第一个扇区被叫做启动扇区。Boot loader启动加载器就放在这里。当BIOS找到一个可启动的设备，它就会加载这512字节大小的启动扇区到0x7c00-0x7dff物理地址的内存中。就像BIOS的加载地址，这些地址也是固定的，为了计算机的标准化。  
  
在计算机发展很久后从CD-ROM中启动系统的技术才出现，计算机设计师们也得以稍微重新思考启动的过程。因此，从CD-ROM启动系统变得有点更复杂而也更强大。CD-ROM的扇区使用了2048字节而不是512，BIOS也能够从设备里加载大得多的启动映像到内存中去。详见["El Torito" Bootable CD-ROM Format Specification](https://pdos.csail.mit.edu/6.828/2018/readings/boot-cdrom.pdf)。  
  
然而在6.828的课程中，我们将使用传统硬件驱动启动机制，也就是说我们的启动加载器必须要放的进小小的512字节里。启动加载器由一个汇编语言源文件boot/boot.s和一个C源文件boot/main.c构成。仔细分析这些源文件确保你明白发生了什么。启动加载器一定要实现两个主要功能：
> 1. 首先，启动加载器将处理器从实模式转换到32位保护模式，因为只有这个模式程序可以访问所有在1MB之上的内存物理地址空间。[PC Assembly Language Book](https://pdos.csail.mit.edu/6.828/2018/readings/pcasm-book.pdf)的1.2.7和1.2.8有简述保护模式，和Intel架构下的详细操作细节。关于这点只需要明白保护模式下分段地址（段地址：段偏移）到物理地址转换不同，转换后段偏移从16位变成了32位。  
> 2. 其次，启动加载器通过x86特定的I/O指令直接访问IDE硬盘设备寄存器，从硬盘中读取内核。如果你想更好的理解这个I/O指令实际做了什么，参考[读物](https://pdos.csail.mit.edu/6.828/2018/reference.html)中"IDE hard drive controller"节。这节课上不需要学太多关于特定设备编程的内容，虽然实际地写设备驱动是OS开发中非常重要的一环，但从概念化和设计化的角度来看是最无吸引力的部分。  
  
在你理解了启动加载器的源码后，看看obj/boot/boot.asm文件，这个文件是GNUmakefile在编译后创建的对启动加载器的反汇编。这个反汇编文件可以很清晰地看到启动加载器的代码分布在在物理内存中何处，让在GDB里追踪启动加载器里发生的事变得更容易。相似的，也有一份对debug很有用的JOS内核的反汇编文件obj/kern/kernel.asm。  
  
GDB地址断点设置指令为b，如`b *0x7c00`在地址0x7C00设置了断点。运行到达断点后，用c或者si指令继续执行：c使得QEMU继续执行到下一个断点，而si N执行接下来的N个指令。  
  
为了检查内存中的指令（在下一个GDB自动打印的要被执行的指令前，立刻），使用x/i命令。语法为`x/Ni ADDR`，N为要连续反汇编的指令数，ADDR为开始反汇编的内存地址。  
  
> + Exercise 3
    查阅[实验工具指导](https://pdos.csail.mit.edu/6.828/2018/labguide.html),尤其是GDB命令的部分。就算你很熟悉GDB了，这里仍有些对OS工作很有帮助的内行GDB命令。  
    在地址0x7c00处设置断点，这里是启动扇区将被加载的地方。继续执行直到断点。用源码和反汇编文件obj/boot/boot.asm追踪boot/boot.s中的代码。也要用GDB的x/i命令反汇编启动加载器的指令串，在obj.boot/boot.asm和GDB中比较启动加载器的源码。    
    跟踪boot/main.c中的bootmain()，然后是readset()。确认每条汇编指令和readsect()中每条声明的联系。追踪完剩下的readset()回到bootmain（），然后确认从硬盘读取待处理扇区的for循环的开始与结束。查明什么代码会在循环结束时运行，并设置断点，然后继续执行到断点。然后单步调试启动加载器剩余的部分。  
  

设置断点  
`(gdb) b *0x7c00`  
执行运行到0x7c00地址处
`(gdb) c`  
在此处跟踪后续三十字节指令  
`(gdb) x/30i 0x7c00`  
得到输出  

```
=> 0x7c00:	cli    
   0x7c01:	cld    
   0x7c02:	xor    %ax,%ax
   0x7c04:	mov    %ax,%ds
   0x7c06:	mov    %ax,%es
   0x7c08:	mov    %ax,%ss
   0x7c0a:	in     $0x64,%al
   0x7c0c:	test   $0x2,%al
   0x7c0e:	jne    0x7c0a
   0x7c10:	mov    $0xd1,%al
   0x7c12:	out    %al,$0x64
   0x7c14:	in     $0x64,%al
   0x7c16:	test   $0x2,%al
   0x7c18:	jne    0x7c14
   0x7c1a:	mov    $0xdf,%al
   0x7c1c:	out    %al,$0x60
   0x7c1e:	lgdtw  0x7c64
   0x7c23:	mov    %cr0,%eax
   0x7c26:	or     $0x1,%eax
   0x7c2a:	mov    %eax,%cr0
   0x7c2d:	ljmp   $0x8,$0x7c32
   0x7c32:	mov    $0xd88e0010,%eax
   0x7c38:	mov    %ax,%es
   0x7c3a:	mov    %ax,%fs
   0x7c3c:	mov    %ax,%gs
   0x7c3e:	mov    %ax,%ss
   0x7c40:	mov    $0x7c00,%sp
   0x7c43:	add    %al,(%bx,%si)
   0x7c45:	call   0x7d13
   0x7c48:	add    %al,(%bx,%si)
```  
对比分析boot.asm/boot.s  
```
start:
  .code16                     # 16位汇编模式
  cli                         # 屏蔽中断
  cld                         # 标志寄存器Flag的方向标志位DF清零

  # 设置重要的数据分段寄存器 (DS, ES, SS).
  xorw    %ax,%ax             # 段寄存器清零
  movw    %ax,%ds             # -> Data Segment
  movw    %ax,%es             # -> Extra Segment
  movw    %ax,%ss             # -> Stack Segment

  # Enable A20:
  #   For backwards compatibility with the earliest PCs, physical
  #   address line 20 is tied low, so that addresses higher than
  #   1MB wrap around to zero by default.  This code undoes this.
  #   IO操作外部设备，为CPU的工作模式从实模式到保护模式转换做准备 
seta20.1:
  inb     $0x64,%al               # Wait for not busy 等待端口可用
  testb   $0x2,%al                # 0x64端口属于键盘控制器804x，名称是控制器读取状态寄存器。
  jnz     seta20.1                

  movb    $0xd1,%al               # 0xd1 -> port 0x64 数据写入 向对象发送指令
  outb    %al,$0x64

seta20.2:
  inb     $0x64,%al               # Wait for not busy 
  testb   $0x2,%al
  jnz     seta20.2

  movb    $0xdf,%al               # 0xdf -> port 0x60 df指令为A20总线使能
  outb    %al,$0x60

  # Switch from real to protected mode, using a bootstrap GDT
  # and segment translation that makes virtual addresses 
  # identical to their physical addresses, so that the 
  # effective memory map does not change during the switch.
  lgdt    gdtdesc                 # 把标识符gdtdesc的值送入全局映射描述符表寄存器GDTR中
  movl    %cr0, %eax
  orl     $CR0_PE_ON, %eax
  movl    %eax, %cr0              # CR0寄存器PE位置1，表示进入保护模式
  
  # Jump to next instruction, but in 32-bit code segment.
  # Switches processor into 32-bit mode. 切换为32位地址模式
  ljmp    $PROT_MODE_CSEG, $protcseg

  .code32                     # 32位汇编模式
protcseg:
  # 设置保护模式下的数据分段寄存器
  movw    $PROT_MODE_DSEG, %ax    # 数据段游标
  movw    %ax, %ds                # -> DS: Data Segment
  movw    %ax, %es                # -> ES: Extra Segment
  movw    %ax, %fs                # -> FS
  movw    %ax, %gs                # -> GS
  movw    %ax, %ss                # -> SS: Stack Segment
  
  # 设置栈指针调用C程序
  movl    $start, %esp
  call bootmain

  # If bootmain returns (it shouldn't), loop.
spin:
  jmp spin

# Bootstrap GDT 全局描述符表
.p2align 2                                # force 4 byte alignment
gdt:
  SEG_NULL				# null seg
  SEG(STA_X|STA_R, 0x0, 0xffffffff)	# code seg
  SEG(STA_W, 0x0, 0xffffffff)	        # data seg

gdtdesc:
  .word   0x17                            # sizeof(gdt) - 1
  .long   gdt                             # address gdt

```  

设置断点并运行至此  
`(gdb) b *0x7c45`  
`Breakpoint 2 at 0x7c45`  
`(gdb) c`  
此处调用bootmain()  
`=> 0x7d15:	push   %ebp`  
跟踪后续指令
```
=> 0x7d15:	push   %ebp         
   0x7d16:	mov    %esp,%ebp           # 修改栈帧界限的信息
   0x7d18:	push   %esi                
   0x7d19:	push   %ebx                # 备份被调用者保存寄存器
   0x7d1a:	push   $0x0                # 输入参数压栈
   0x7d1c:	push   $0x1000
   0x7d21:	push   $0x10000
   0x7d26:	call   0x7cdc              # 跳转执行readseg()
```
跳转到readseg()
```
=> 0x7cdc:	push   %ebp
   0x7cdd:	mov    %esp,%ebp
   0x7cdf:	push   %edi
   0x7ce0:	push   %esi
   0x7ce1:	mov    0x10(%ebp),%edi
   0x7ce4:	push   %ebx
   0x7ce5:	mov    0xc(%ebp),%esi
   0x7ce8:	mov    0x8(%ebp),%ebx
   0x7ceb:	shr    $0x9,%edi
   0x7cee:	add    %ebx,%esi
   0x7cf0:	inc    %edi
   0x7cf1:	and    $0xfffffe00,%ebx
   0x7cf7:	cmp    %esi,%ebx            
   0x7cf9:	jae    0x7d0d
   0x7cfb:	push   %edi
   0x7cfc:	push   %ebx
   0x7cfd:	inc    %edi
   0x7cfe:	add    $0x200,%ebx
   0x7d04:	call   0x7c7c               #调用readsect()
```
继续追踪  
```
=> 0x7c7c:	push   %ebp
   0x7c7d:	mov    %esp,%ebp
   0x7c7f:	push   %edi
   0x7c80:	mov    0xc(%ebp),%ecx
   0x7c83:	call   0x7c6a               #调用waitdisk()
```
至此循环读取扇区128次  
返回至bootmain()中  
判断ELF是否有效
```
=> 7d2b:	83 c4 0c             	add    $0xc,%esp
   7d2e:	81 3d 00 00 01 00 7f 	cmpl   $0x464c457f,0x10000
   7d35:	45 4c 46 
   7d38:	75 37                	jne    7d71 <bootmain+0x5c>
```
之后循环读取ELF头中记录的程序分段  
循环结束后执行`((void (*)(void)) (ELFHDR->e_entry))();`  
即，断点位置设置在0x10018  
`=> 7d6b:	ff 15 18 00 01 00    	call   *0x10018`  

对比分析boot/main.c
```
#define SECTSIZE	512
#define ELFHDR		((struct Elf *) 0x10000) // scratch space

void readsect(void*, uint32_t);
void readseg(uint32_t, uint32_t, uint32_t);

void
bootmain(void)
{
	struct Proghdr *ph, *eph;

	// read 1st page off disk 读取盘符中的第一页
    // 把操作系统映像文件的elf头部读取出来放入内存中
	readseg((uint32_t) ELFHDR, SECTSIZE*8, 0);

	// is this a valid ELF? 判断是否为有效的ELF
	if (ELFHDR->e_magic != ELF_MAGIC)
		goto bad;

	// load each program segment (ignores ph flags) 加载每个程序分段
	ph = (struct Proghdr *) ((uint8_t *) ELFHDR + ELFHDR->e_phoff);
	eph = ph + ELFHDR->e_phnum;
	for (; ph < eph; ph++)
        // 遍历ph读取程序分段
		// p_pa is the load address of this segment (as well
		// as the physical address)
        // p_pa是这个分段的加载地址（也是物理地址）
		readseg(ph->p_pa, ph->p_memsz, ph->p_offset);

	// call the entry point from the ELF header 从ELF头调用入口指针
	// note: does not return! 注意不返回
	((void (*)(void)) (ELFHDR->e_entry))();

bad:
	outw(0x8A00, 0x8A00);
	outw(0x8A00, 0x8E00);
	while (1)
		/* do nothing */;
}

// Read 'count' bytes at 'offset' from kernel into physical address 'pa'.
// Might copy more than asked
void
readseg(uint32_t pa, uint32_t count, uint32_t offset)
{
	uint32_t end_pa;

	end_pa = pa + count;

	// round down to sector boundary  扇区边界向下取整
	pa &= ~(SECTSIZE - 1);

	// translate from bytes to sectors, and kernel starts at sector 1 从字节到扇区的转换，内核开始于扇区1
	offset = (offset / SECTSIZE) + 1;

	// If this is too slow, we could read lots of sectors at a time.
	// We'd write more to memory than asked, but it doesn't matter --
	// we load in increasing order.
	while (pa < end_pa) {
		// Since we haven't enabled paging yet and we're using
		// an identity segment mapping (see boot.S), we can
		// use physical addresses directly.  This won't be the
		// case once JOS enables the MMU.
        // 由于还没有使能MMU分页功能，所以采取一种一致性分段映射，以直接使用物理地址。
        // 循环读取分区
		readsect((uint8_t*) pa, offset);
		pa += SECTSIZE;
		offset++;
	}
}

void
waitdisk(void)
{
	// wait for disk reaady
	while ((inb(0x1F7) & 0xC0) != 0x40)
		/* do nothing */;
}

void
readsect(void *dst, uint32_t offset)
{
	// wait for disk to be ready
	waitdisk();

	outb(0x1F2, 1);		// count = 1
	outb(0x1F3, offset);
	outb(0x1F4, offset >> 8);
	outb(0x1F5, offset >> 16);
	outb(0x1F6, (offset >> 24) | 0xE0);
	outb(0x1F7, 0x20);	// cmd 0x20 - read sectors

	// wait for disk to be ready
	waitdisk();

	// read a sector
	insl(0x1F0, dst, SECTSIZE/4);
}
```

> + Q1 在什么时候处理器开始运行于32bit模式？到底是什么把CPU从16位切换为32位工作模式？ 
  在boot.S文件中，计算机首先工作于实模式，此时是16bit工作模式。当运行完 ” ljmp $PROT_MODE_CSEG, $protcseg ” 语句后，正式进入32位工作模式。根本原因是此时CPU工作在保护模式下。
> + Q2 boot loader中执行的最后一条语句是什么？内核被加载到内存后执行的第一条语句又是什么？
  boot loader执行的最后一条语句是bootmain子程序中的最后一条语句 ” ((void (*)(void)) (ELFHDR->e_entry))(); “，即跳转到操作系统内核程序的起始指令处。这个第一条指令位于/kern/entry.S文件中，第一句 movw $0x1234, 0x472 
> + Q3 内核的第一条指令在哪里？ 
  位于/kern/entry.S文件中。
> + Q4 boot loader是如何知道它要读取多少个扇区才能把整个内核都送入内存的呢？在哪里找到这些信息？
  首先关于操作系统一共有多少个段，每个段又有多少个扇区的信息位于操作系统文件中的Program Header Table中。这个表中的每个表项分别对应操作系统的一个段。并且每个表项的内容包括这个段的大小，段起始地址偏移等等信息。所以如果我们能够找到这个表，那么就能够通过表项所提供的信息来确定内核占用多少个扇区。那么关于这个表存放在哪里的信息，则是存放在操作系统内核映像文件的ELF头部信息中。  
  
### 加载内核
之后将进入启动加载器的C语言部分，在文件boot/main.c中。在此之前，课程推荐停下来复习有关C语言编程的基础。  
> + Exercise 4
    阅读有关C语言中用指针编程的资料，Mit推荐《The C Programming Language》,Brian Kernighan and Dennis Ritchie著。  
    详读资料，下载[pointers.c](https://pdos.csail.mit.edu/6.828/2018/labs/lab1/pointers.c)并运行，确保自己理解所有打印的值从何而来。具体地说，是理解行1，6的指针地址从何而来，所有打印在行2-4的值是如何到达这里的，还有为何行5打印的值看起来被破坏了。  

```
1: a = 000000000061FDC0, b = 0000000000A61400, c = 0000000000000010
2: a[0] = 200, a[1] = 101, a[2] = 102, a[3] = 103
3: a[0] = 200, a[1] = 300, a[2] = 301, a[3] = 302
4: a[0] = 200, a[1] = 400, a[2] = 301, a[3] = 302
5: a[0] = 200, a[1] = 128144, a[2] = 256, a[3] = 302
6: a = 000000000061FDC0, b = 000000000061FDC4, c = 000000000061FDC1
```
  
为了完全理解boot/main.c需要知道ELF二进制文件是什么。当你编译链接一个像JOS内核语言一样的C程序，编译器转换每个C源文件(.c)到一个object('.o'）文件,其中包含着的硬件能够读取的的汇编指令转编的二进制码。链接器之后整合所有的编译object文件到一个二进制映射文件中，如obj/kern/kernel。在这里是一个ELF格式的二进制文件，代表可执行和链接格式（Executable and Linkable Format)。  
ELF的完整信息见[the ELF specification](https://pdos.csail.mit.edu/6.828/2018/readings/elf.pdf),但这节课不必深入了解其细节。尽管总的来说这个格式相当强大而复杂，但它其中大部分复杂的部分是为了支持共享库的动态加载，而不是这节课要做的。  
为了完成课题，你可以把ELF可执行理解为附有加载信息，跟着数个相邻的代码块或者要被加载进内存指定地址中的数据块的程序分段的标头。启动加载器不会修改代码或数据，只会把它们加载进内存并执行。  
一个ELF二进制文件从一个固定长度的ELF标头开始，后面是一个列出所有要被加载的程序分段的可变长程序标头。这些ELF标头的C定义在inc/elf.h中。我们需要关心的程序分段有：  
> + .text:程序可执行指令
> + .rodata:只读数据，如C编译器产生的ASCII字符常量。（然而我们不会麻烦地设置硬件来禁止写操作。）
> + .data:这个数据分段保有程序地初始化数据，如像`int x = 5`这样声明地全局变量。  
  
当链接器处理一个程序地内存分布时，它会为未初始化的全局变量预留空间，如内存中紧跟着.data的一个叫.bss的分段里的`int x`。C语言要求未初始化的全局变量有一个初始值0，因此就没必要在ELF二进制文件中为.bss存储内容；取而代之的是，链接器只是记录.bss分段的地址和大小。加载器或者程序自身必须安排.bss分段置零。  
检查附有内核中所有分段的名字大小和链接地址完整列表的指令：  
`objdump -h obj/kern/kernel`  
输出  
```
obj/kern/kernel:     file format elf32-i386

Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .text         000019e9  f0100000  00100000  00001000  2**4
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
  1 .rodata       000006c0  f0101a00  00101a00  00002a00  2**5
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  2 .stab         00003b95  f01020c0  001020c0  000030c0  2**2
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  3 .stabstr      00001948  f0105c55  00105c55  00006c55  2**0
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  4 .data         00009300  f0108000  00108000  00009000  2**12
                  CONTENTS, ALLOC, LOAD, DATA
  5 .got          00000008  f0111300  00111300  00012300  2**2
                  CONTENTS, ALLOC, LOAD, DATA
  6 .got.plt      0000000c  f0111308  00111308  00012308  2**2
                  CONTENTS, ALLOC, LOAD, DATA
  7 .data.rel.local 00001000  f0112000  00112000  00013000  2**12
                  CONTENTS, ALLOC, LOAD, DATA
  8 .data.rel.ro.local 00000044  f0113000  00113000  00014000  2**2
                  CONTENTS, ALLOC, LOAD, DATA
  9 .bss          00000648  f0113060  00113060  00014060  2**5
                  CONTENTS, ALLOC, LOAD, DATA
 10 .comment      00000029  00000000  00000000  000146a8  2**0
                  CONTENTS, READONLY
```
有很多没有被列出来的分段，但对课题不是那么重要。大部分其他分段是存放debug信息的，一般都被包含在程序的可执行文件里但不会被程序加载器加载到内存中。  
注意.text分段里的VMA（link address)和LMA(load address)。一个分段的加载地址是这个分段应该被加载在内存中的地址。  
一个分段的链接地址是整个分段应该从何处被执行的地址。链接器用数种方式将链接地址编码成二进制，例如当代码需要一个全局变量的地址时，如果它不是从它被链接的地方执行的，其二进制码就不会起效。（生成没有这些绝对地址的位置无关的代码是可能的。这通常在现代共享库中大量使用，但它是以运行效率和复杂度为代价的，所以课题不会使用这种技术。）  
通常，链接和加载地址是一样的，例如，检查启动加载器中的.text分段：
`objdump -h obj/boot/boot.out`    
输出  
```
obj/boot/boot.out:     file format elf32-i386

Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .text         00000186  00007c00  00007c00  00000074  2**2
                  CONTENTS, ALLOC, LOAD, CODE
  1 .eh_frame     000000a8  00007d88  00007d88  000001fc  2**2
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  2 .stab         0000087c  00000000  00000000  000002a4  2**2
                  CONTENTS, READONLY, DEBUGGING
  3 .stabstr      00000925  00000000  00000000  00000b20  2**0
                  CONTENTS, READONLY, DEBUGGING
  4 .comment      00000029  00000000  00000000  00001445  2**0
                  CONTENTS, READONLY
```
启动加载器使用ELF程序标头来决定如何加载分段，程序标头详述了ELF对象的那一部分要被加载进内存和因该占据的目标地址。检查程序标头指令：  
`objdump -x obj/kern/kernel`  

输出  
```
obj/kern/kernel:     file format elf32-i386
obj/kern/kernel
architecture: i386, flags 0x00000112:
EXEC_P, HAS_SYMS, D_PAGED
start address 0x0010000c

Program Header:
    LOAD off    0x00001000 vaddr 0xf0100000 paddr 0x00100000 align 2**12
         filesz 0x0000759d memsz 0x0000759d flags r-x
    LOAD off    0x00009000 vaddr 0xf0108000 paddr 0x00108000 align 2**12
         filesz 0x0000b6a8 memsz 0x0000b6a8 flags rw-
   STACK off    0x00000000 vaddr 0x00000000 paddr 0x00000000 align 2**4
         filesz 0x00000000 memsz 0x00000000 flags rwx

Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .text         000019e9  f0100000  00100000  00001000  2**4
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
  1 .rodata       000006c0  f0101a00  00101a00  00002a00  2**5
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  2 .stab         00003b95  f01020c0  001020c0  000030c0  2**2
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  3 .stabstr      00001948  f0105c55  00105c55  00006c55  2**0
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  4 .data         00009300  f0108000  00108000  00009000  2**12
                  CONTENTS, ALLOC, LOAD, DATA
  5 .got          00000008  f0111300  00111300  00012300  2**2
                  CONTENTS, ALLOC, LOAD, DATA
  6 .got.plt      0000000c  f0111308  00111308  00012308  2**2
                  CONTENTS, ALLOC, LOAD, DATA
  7 .data.rel.local 00001000  f0112000  00112000  00013000  2**12
                  CONTENTS, ALLOC, LOAD, DATA
  8 .data.rel.ro.local 00000044  f0113000  00113000  00014000  2**2
                  CONTENTS, ALLOC, LOAD, DATA
  9 .bss          00000648  f0113060  00113060  00014060  2**5
                  CONTENTS, ALLOC, LOAD, DATA
 10 .comment      00000029  00000000  00000000  000146a8  2**0
                  CONTENTS, READONLY
SYMBOL TABLE:
f0100000 l    d  .text	00000000 .text
f0101a00 l    d  .rodata	00000000 .rodata
f01020c0 l    d  .stab	00000000 .stab
f0105c55 l    d  .stabstr	00000000 .stabstr
f0108000 l    d  .data	00000000 .data
f0111300 l    d  .got	00000000 .got
f0111308 l    d  .got.plt	00000000 .got.plt
f0112000 l    d  .data.rel.local	00000000 .data.rel.local
f0113000 l    d  .data.rel.ro.local	00000000 .data.rel.ro.local
f0113060 l    d  .bss	00000000 .bss
00000000 l    d  .comment	00000000 .comment
00000000 l    df *ABS*	00000000 obj/kern/entry.o
f010002f l       .text	00000000 relocated
f010003e l       .text	00000000 spin
00000000 l    df *ABS*	00000000 entrypgdir.c
00000000 l    df *ABS*	00000000 init.c
00000000 l    df *ABS*	00000000 console.c
f01001c0 l     F .text	0000001f serial_proc_data
f01001df l     F .text	0000004b cons_intr
f0113080 l     O .bss	00000208 cons
f010022a l     F .text	00000132 kbd_proc_data
f0113060 l     O .bss	00000004 shift.1338
f0101bc0 l     O .rodata	00000100 shiftcode
f0101ac0 l     O .rodata	00000100 togglecode
f0113000 l     O .data.rel.ro.local	00000010 charcode
f010035c l     F .text	00000216 cons_putc
f0113288 l     O .bss	00000002 crt_pos
f0113290 l     O .bss	00000004 addr_6845
f011328c l     O .bss	00000004 crt_buf
f0113294 l     O .bss	00000001 serial_exists
f0111200 l     O .data	00000100 normalmap
f0111100 l     O .data	00000100 shiftmap
f0111000 l     O .data	00000100 ctlmap
00000000 l    df *ABS*	00000000 monitor.c
f0113010 l     O .data.rel.ro.local	00000018 commands
00000000 l    df *ABS*	00000000 printf.c
f01009f0 l     F .text	00000022 putch
00000000 l    df *ABS*	00000000 kdebug.c
f0100a5d l     F .text	000000f0 stab_binsearch
00000000 l    df *ABS*	00000000 printfmt.c
f0100d59 l     F .text	000000ca printnum
f0100e23 l     F .text	0000001d sprintputch
f0113028 l     O .data.rel.ro.local	0000001c error_string
f0101292 l       .text	00000000 .L22
f0100f34 l       .text	00000000 .L23
f0101281 l       .text	00000000 .L25
f0100ef3 l       .text	00000000 .L26
f0100ebf l       .text	00000000 .L67
f0100f1c l       .text	00000000 .L27
f0100ec8 l       .text	00000000 .L28
f0100ed1 l       .text	00000000 .L29
f0100f54 l       .text	00000000 .L30
f01010c9 l       .text	00000000 .L31
f0100f6e l       .text	00000000 .L32
f0100f48 l       .text	00000000 .L33
f01011a2 l       .text	00000000 .L34
f01011c2 l       .text	00000000 .L35
f0100fc3 l       .text	00000000 .L36
f0101153 l       .text	00000000 .L37
f010122f l       .text	00000000 .L38
00000000 l    df *ABS*	00000000 readline.c
f01132a0 l     O .bss	00000400 buf
00000000 l    df *ABS*	00000000 string.c
00000000 l    df *ABS*	00000000 
f0111308 l     O .got.plt	00000000 _GLOBAL_OFFSET_TABLE_
f0100d55 g     F .text	00000000 .hidden __x86.get_pc_thunk.cx
f010000c g       .text	00000000 entry
f0101468 g     F .text	00000020 strcpy
f010059a g     F .text	0000001d kbd_intr
f0100883 g     F .text	0000000a mon_backtrace
f0100106 g     F .text	0000006a _panic
f01000a6 g     F .text	00000060 i386_init
f01015f6 g     F .text	00000068 memmove
f010131a g     F .text	0000001a snprintf
f0100e5d g     F .text	0000045e vprintfmt
f01005b7 g     F .text	0000005a cons_getc
f0100a49 g     F .text	00000014 cprintf
f010165e g     F .text	00000013 memcpy
f0101334 g     F .text	000000fd readline
f0110000 g     O .data	00001000 entry_pgtable
f0100040 g     F .text	00000066 test_backtrace
f01012bb g     F .text	0000005f vsnprintf
f0113060 g       .bss	00000000 edata
f0100611 g     F .text	00000122 cons_init
f010075e g     F .text	00000000 .hidden __x86.get_pc_thunk.ax
f0105c54 g       .stab	00000000 __STAB_END__
f0105c55 g       .stabstr	00000000 __STABSTR_BEGIN__
f01018d0 g     F .text	00000119 .hidden __umoddi3
f0100572 g     F .text	00000028 serial_intr
f01017b0 g     F .text	00000116 .hidden __udivdi3
f0100754 g     F .text	0000000a iscons
f01016c7 g     F .text	000000de strtol
f0101449 g     F .text	0000001f strnlen
f0101488 g     F .text	00000022 strcat
f01136a4 g     O .bss	00000004 panicstr
f01136a0 g       .bss	00000000 end
f0100170 g     F .text	0000004c _warn
f010158d g     F .text	0000001c strfind
f01019e9 g       .text	00000000 etext
0010000c g       .text	00000000 _start
f01014d7 g     F .text	00000037 strlcpy
f0101534 g     F .text	00000038 strncmp
f01014aa g     F .text	0000002d strncpy
f01001bc g     F .text	00000000 .hidden __x86.get_pc_thunk.bx
f0101671 g     F .text	00000039 memcmp
f0100733 g     F .text	00000010 cputchar
f01015a9 g     F .text	0000004d memset
f0100743 g     F .text	00000011 getchar
f0100e40 g     F .text	0000001d printfmt
f010759c g       .stabstr	00000000 __STABSTR_END__
f010150e g     F .text	00000026 strcmp
f0100b4d g     F .text	00000208 debuginfo_eip
f0100a12 g     F .text	00000037 vcprintf
f0110000 g       .data	00000000 bootstacktop
f0112000 g     O .data.rel.local	00001000 entry_pgdir
f0108000 g       .data	00000000 bootstack
f01020c0 g       .stab	00000000 __STAB_BEGIN__
f0101431 g     F .text	00000018 strlen
f010156c g     F .text	00000021 strchr
f01007b2 g     F .text	000000d1 mon_kerninfo
f010088d g     F .text	00000163 monitor
f01016aa g     F .text	0000001d memfind
f0100762 g     F .text	00000050 mon_help
```
objdump输出里"Program Headers"下列出的就是程序标头。ELF对象中要被加载进内存的区域被标注为"LOAD"。每个程序标头的其他信息也一并展示了，如虚拟地址（vaddr），物理地址(paddr)和加载区域大小（memsz和filesz）。  
回到/boot/main.c，每个程序标头的`ph->p_pa`域保存了分段的目标物理地址（在这里，它这点是一个物理地址，尽管ELF对于此域的实际意义的说明相当模糊）。  
BIOS加载启动扇区到内存中以地址0x7c00开始的地方，故这就是启动扇区的加载地址。这也是启动扇区从哪里执行的地方，而且也是它的链接地址。通过添加`-Ttext 0x7c00`到boot/Makefrag里的链接器来设置链接地址，使得链接器在生成码中能产生正确的内存地址。  
> + Exercise 5
    Lab下`make clean`清除编译，修改链接地址，再make重新编译，再次跟踪启动加载器的头几个指令，找到修改链接地址后第一条出错的指令，看看会发生什么。最后改回原来的链接地址重新编译。  
  
目光回到内核的加载和链接地址。不像启动加载器，这两个地址是不同的：内核告诉启动加载器把它加载到内存中的低地址（1兆字节），但它因该从一个高地址被执行。下一节深入如何使其奏效。  
除了分段信息，对我们来说ELF标头中还有一个很重要的域e_entry。这个域保存了程序的进入指针的链接地址，即程序因该从哪里开始执行的程序文本分段内存地址。  
检查进入指针`objdump -f obj/kern/kernel`  
输出  
```
obj/kern/kernel:     file format elf32-i386
architecture: i386, flags 0x00000112:
EXEC_P, HAS_SYMS, D_PAGED
start address 0x0010000c
```
现在因该有能力明白boot/main.c中最简单的ELF标头。它从硬盘中读取每个内核分段到内存中的加载地址，然后跳转到内核的进入指针地址。  
> + Exercise 6
    可以使用GDB的-x指令来检查内存。这里了解`x/Nx ADDR`打印ADDR地址后N个字的内存，注意-x小写。警告，一个字的大小不是统一标准。在GNU汇编中，一个字是两字节（xorw中的w代表word,意味着两字节）。  
重置机器，退出QEMU/GDB再重启。检查内存中0x0010 0000后8个字的内容，一次在BIOS进入启动加载器时，另一次在启动加载器进入内核时。看看发生了什么。  

断点设置  
`b *0x7c00`  
`b *0x7d6b`  
第一次检查  
```  
0x100000:	0x00000000	0x00000000	0x00000000	0x00000000
0x100010:	0x00000000	0x00000000	0x00000000	0x00000000
```
第二次检查
```
0x100000:	0x1badb002	0x00000000	0xe4524ffe	0x7205c766
0x100010:	0x34000004	0x2000b812	0x220f0011	0xc0200fd8
```
