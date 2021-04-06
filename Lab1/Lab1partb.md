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
  
#### Exercise 3

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