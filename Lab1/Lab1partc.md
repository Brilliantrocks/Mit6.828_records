# Lab1
## Part c.The Kernel  内核

现在我们将开始检查一些最小JOS内核更多的细节（也终于能写一些代码了）。就像启动加载器，内核开始于一些汇编语言码，它们设置了一些能够正确执行C语言代码的东西。  
### 使用虚拟内存来处理位置依赖性

之前当你检查启动加载器的链接和加载地址时它们时一致的，但在内核的链接地址和加载地址差的就很大。具体参见上一part最后的部分。（链接内核比启动加载器更加复杂，所以链接和加载带着放在kern/kernel.ld最前面。）  
  
操作系统内核通常可能被链接和运行在非常高的虚拟地址，例如0xf0100000，这是为了给用户程序使用而保留了了处理器虚拟地址空间中低位的部分。这样安排的理由稍后会在实验中澄清。  
  
许多机器在0xf0100000之上没有物理内存了，所以我们不能指望能把内核放在那里。作为替代，我们将实验处理器的内存管理硬件来映射虚拟地址0xf0100000（即内核代码应该从何处被运行的链接地址）。对于这种方法，尽管内核的虚拟地址已经足够高来为用户进程预留大量空间，它仍会被加载到计算机RAM中物理内存1MB指针的位置，刚好在BIOS ROM之上。这种方式要求计算机至少有数个兆字节的物理内存（如此物理地址0x00100000才能有效），而这只有1990年后制造的计算机才能办到。  
  
事实上，在下一个实验中，我们将映射整个底部256MB的计算机物理地址空间，分别物理地址0x00000000-0x0fffffff，和在虚拟地址0xf0000000-0xffffffff。现在你应该明白为何JOS只能是由前256MB的物理内存了。  
  
现在，我们首先映射前4MB的物理内存来熟悉有一下。通过使用放在kerm/entrypgdir.c库的手写的，静态初始化的页目录和页表来开始。目前不需要理解这些实现的细节，看看它完成的效果就好。在kern/entry.c设置了CR0_PG位前，内存引用和被当作和物理地址时一样的（严格来讲，它们都是线性地址，但/boot/boot.s设置了一种从线性地址到物理地址的标识映射，而我们永远不会改变这一事实）。一旦CR0_PG位被设置，内存引用就是那些被虚拟内存硬件转换成物理地址的虚拟地址。entry_pgdir转换0xf0000000-0xf0400000的虚拟地址到0x00000000-0x00400000的物理地址，和0x00000000-0x00400000的虚拟内存到0x0000000-0x00400000的物理地址。任何不在两段范围内的虚拟地址会引起一个硬件意外，而在我们还没有设置中断处理前，这会导致QEMU丢出机器状态信息然后退出（或者无尽的重启如果你没用6.828补丁过的版本的话）。  
  
> + Exercise 7
    使用QEMU和GDB来跟踪JOS内核，在movl %eax,%cr0这一步停下。检查0x00100000和0xf0100000的内存。然后单步调试，再次检查0x00100000和0xf0100000的内存。确保理解发生了什么。
    如果映射不在正确位置，在这映射建立之后失效的第一个指令是什么？注释掉kern/entry.S里的movl %eax,%cr0，追踪这一状况。  

由之前的实验得知内核开始于0x0010000c处  
`b *0x001000c`  
检查后续指令  
```
=> 0x10000c:	movw   $0x1234,0x472
   0x100015:	mov    $0x112000,%eax
   0x10001a:	mov    %eax,%cr3
   0x10001d:	mov    %cr0,%eax
   0x100020:	or     $0x80010001,%eax
   0x100025:	mov    %eax,%cr0
   0x100028:	mov    $0xf010002f,%eax
   0x10002d:	jmp    *%eax
```

si至  
`=> 0x100025:	mov    %eax,%cr0`  
检查内存  
`x/4x 0x00100000`  
`x/4x 0xf0100000`  
输出
`0x100000:	0x1badb002	0x00000000	0xe4524ffe	0x7205c766`  
`0xf0100000 <_start+4026531828>:	0x00000000	0x00000000	0x00000000	0x00000000`  
si执行mov,再次检查内存  
`0x100000:	0x1badb002	0x00000000	0xe4524ffe	0x7205c766`  
`0xf0100000 <_start+4026531828>:	0x1badb002	0x00000000	0xe4524ffe	0x7205c766`  
内存一致，说明映射已经建立。  
  
将`%movl %eax, %cr0`注释掉后重新编译调试，
`=> 0x10002a:	jmp    *%eax`   
该指令后报出错误，因为eax指向的跳转地址为f010002c，是虚拟地址。在没有建立映射时无法正常访问虚拟地址，超出了内存的物理地址。  
  
### 格式化输出到控制台
大部分人把像printf()这样的函数想的理所当然，有的适合甚至认为它们是C语言中的原语。但在一个操作系统内核中，我们得自己开发所有的I/O操作。  
通读kern/printf.c,lib/printfmt.c和kern/console.c然后确保自己明白它们之间的联系。后续的实验会说明为何prinfmt.c放在独立的lib目录里。  
> + Exercise 8
    课题中删去了代码中的一小段，一小段使用"%o"格式打印八进制数的重要代码。找到并修复这一段代码。  
  
位于printfmt.c中vprintfmt()函数里的一个switch-case，原代码为
```
		// (unsigned) octal
		case 'o':
			// Replace this with your code.
			putch('X', putdat);
			putch('X', putdat);
			putch('X', putdat);
			break;
```
修正后  
```
		// (unsigned) octal
		case 'o':
			// Replace this with your code.
			num = getuint(&ap, lflag);
			base = 8;
			goto number;
```  
回答下列问题  
> + 1 解释printf.c和console.c的交互，准确的说，console.c输出了什么函数？它是怎么被printf.c调用的？
      printf.c在putch()函数中内部调用了console.c里的cputchar()函数。
> + 2 解释console.c中的下列行。
      ```
      1      if (crt_pos >= CRT_SIZE) {
      2              int i;
      3              memmove(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
      4              for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
      5                      crt_buf[i] = 0x0700 | ' ';
      6              crt_pos -= CRT_COLS;
      7      }
      自动换行。
> + 3 下面的问题可能需要查阅课程2的笔记，这些笔记覆盖了GCC在x86上调用的惯例。一步一步追踪下列代码行的执行
      int x =1, y = 3, z = 4;
      cprintf("x %d, y %x, z %d\n", x, y, z);
      在cprinf()调用时，fmt指向什么？ap指向什么?
      以执行顺序列出cons_putc,va_arg和vcprintf的调用。也要把cons_putc的参数列出来。对于va_arg，列出ap调用前后的指向，对于vcprintf列出它的两个参数值。
      cprintf调用时fmt指向参数字符串，即"x %d, y %x, z %d\n"。ap指向变参列表中当前处理的参数。
      调用顺序 vcprintf(fmt, ap),fmt指向"x %d, y %x, z %d\n"，ap为{x,y,z};字符串中首先为非格式化参数，则先调用cons_putc打印，第一个参数为字符"x";依次检测打印至%标志，调用格式化过程，第一个va_arg调用的参数ap指向x,调用后ap指向y。
> + 4 运行下列代码
      unsigned int i = 0x00646c72;
      cprintf("H%x Wo%s", 57616, &i);
      通过前面的联系解释这个输出是如何一步一步到达这里的。需要用到ASCII对照表。
      这个输出依赖于x86的小端字节序，如果替换为大端字节序要设置一个什么样的i值来获得同样的输出？需要改变57616的值吗？
      输出为 `He110，World `
      cprintf()首先一路调用到vprintfmt（）进行格式化处理，参数列表中第一个参数为%x格式，进入case 'x'处理，嵌套调用va_arg获得第一个参数57616，经过printnum(）输出为十六进制数e110；然后打印非格式化字符到下一个%处，进入case 's'，调用va_arg获得第二个参数i，打印i的值记录的地址上的字符串，将整形值以小端字节序分割为地址值"72""6c""64"打印，对照ASCII码表即"rld "。
      当改变为大端字节序时，为了获得同样的输出需要将i设置为0x726c6400。改变字节序不会导致存储值的改变，所以57616不需要改变。
> + 5 在下行代码中，'y='后会打印什么？（注意答案不是一个具体的值）为何会这样？
       cprintf("x=%d y=%d", 3);
      在进行到case 'd'处理时，嵌套调用va_arg（）返回了一个不确定的值。
> + 6 假设GCC改变了它的调用惯例，以声明顺序压栈参数，最后的参数最后压入，要如何改变cprintf或者它的接口使得它还能够传递变参？
      需要传递输入的变参数量，以确定向前读取内存中参数的数量。
> + challenge 
    改进控制台使其能够以不同颜色打印文本。6.828参考页里有大量信息。

### 栈
在这个实验最后的练习里，将探索C语言使用x86的栈的更多细节。在这个过程中写一个有用的新内核检测函数，打印栈的回溯，即一系列从嵌套的call指令里保存的，指向当前执行的指令指针（Instruction Pointer/IP)值。  
> + Exercise 9
    查明内核在哪里初始化它的栈，和实际上位于内存中的哪里。内核是如何为它的栈保留空间的？这个栈指针初始化时指向到这块保留空间的哪里？
    entry.s中
        .data

	    .p2align	PGSHIFT		# force page alignment
	    .globl		bootstack
    bootstack:
	    .space		KSTKSIZE
	    .globl		bootstacktop   
    bootstacktop:
    
    entry.s用entrypgdir临时启用的低位内存映射，开启页表分配功能，为栈分配了KSTKSIZE的空间，其大小为8页即32KB。

    分析kernel.asm，找到'movl	$(bootstacktop),%esp`，这条指令设置了栈顶指针，根据实际指令'f0100034:	bc 00 00 11 f0       	mov    $0xf0110000,%esp'得知栈顶指针指向0xf0110000，其实际物理内存地址为0x00110000。栈底指针由'	movl	$0x0,%ebp'指令设置为0。

    综上栈位于0xf0108000-0xf0110000，实际物理内存地址为0x00108000-0x00110000的32KB空间。
   
  
x86栈指针（esp寄存器）指向栈的最低位，即正在使用的部分。每个低于这个位置的，为栈保留的区域都是空闲的。压值入栈首先对栈指针减，然后再将值写入栈指针指向的地方。取值出栈首先读取栈指针指向的值，然后再对栈指针加。在32位模式中，栈只能保存32位的值，esp永远能被4整除，不同的x86指令，如call，都是硬件绑定调用栈指针寄存器。  
ebp寄存器（基指针），相对的，主要被软件连到栈上。进入一个C函数时，这个函数的序码一般保存了前一个函数的基指针，通过将其入栈的方式，然后将本函数的基指针赋给ebp寄存器。如果一个程序的所有函数都遵循这一惯例，那么就能够通过栈保存的ebp链反向追踪查明里哪个嵌套函数语句调用使得程序运行到这里。这个功能十分的有用，例如，当一个实际函数因为被传递了错误的参数导致assert错误或panic，但你不确定谁传递了错误参数时。栈反向追踪能让你找到非法函数。  
> + Exercise 10
    为了熟悉x86上C语言调用的惯例，在obj/kern/kernel.asm里找到函数test_backtrace的地址，设置一个断点，然后检测它内核启动后的每次调用。test_backtrace的每个递归嵌套中有多少32位字被压入栈里？它们是什么？
    注意，为了这个练习的正确进行，应该使用QEMU的课题补丁版本。不然就要手动转换所有断点和内存地址到线性地址。
    查找kernel.asm得知函数地址为0xf0100040。设置断点，检查esp,ebp寄存器。
    b *0xf0100040
    c
    info reg
    得到
    esp            0xf010ffdc	0xf010ffdc
    ebp            0xf010fff8	0xf010fff8
        push   %ebp            之前的ebp的值入栈后
    esp            0xf010ffd8	0xf010ffd8
    ebp            0xf010fff8	0xf010fff8
     	mov    %esp,%ebp       用当前的esp更新本函数的ebp后
    esp            0xf010ffd8	0xf010ffd8
    ebp            0xf010ffd8	0xf010ffd8
        push   %esi            esi值入栈,保存原调用函数可能使用的寄存器值
    esp            0xf010ffd4	0xf010ffd4
    ebp            0xf010ffd8	0xf010ffd8
    	push   %ebx            ebx值入栈，同上
    esp            0xf010ffd0	0xf010ffd0
    ebp            0xf010ffd8	0xf010ffd8
        call   0xf01001bc <__x86.get_pc_thunk.bx> 将栈顶ebx值赋回ebx,
       	add    $0x112be,%ebx    获取参数

        mov    0x8(%ebp),%esi   获取参数
     ......
    执行到递归调用自身
        sub    $0xc,%esp        栈指针下移预留空间
        lea    -0x1(%esi),%eax  计算eax-1的参数值
        push   %eax             参数入栈
        call   f0100040 <test_backtrace>  进入函数头
    由此可见每次递归调用进行了4次push入栈，都是保存的上一层调用函数的参数。     
  
上面的练习会给你实现一个栈反向追踪函数的所需的信息，可以叫mon_backtrace()。这样的一个函数原型已经放在kern/monitor.c里等待实现。可以完全使用c语言实现，但inc/x86的read_ebp()函数也很有用。这个函数同样应该被关联到内核监视器指令列表里好让用户交互地引用。  
这个反向追踪函数应该展示一系下面的列函数调用框架格式：  
```
Stack backtrace:
  ebp f0109e58  eip f0100a62  args 00000001 f0109e80 f0109e98 f0100ed2 00000031
  ebp f0109ed8  eip f01000d6  args 00000000 00000000 f0100058 f0109f28 00000061
  ...
```
每行包含一个ebp,eip和参数列。ebp地值指示了函数在栈中地基指针，即栈指针刚进入函数并由函数序码设置基指针时的栈中位置。eip值为函数的返回指令指针，即当函数返回时指向的指令地址。返回函数指针理所当然地指向了call指令后地下一个指令。最后，五个列在arg后地16进制值是函数的头五个参数，在函数被调用前刚好被压入栈中。如果被调用的函数没有五个参数，则五个值并不全是有效的。  
  
打印的第一行反应了当前执行函数，即被称作为mon_backtrace的它自己，第二行反应了调用mon_backtrace的函数，第三行反映了更上一层的调用函数，如此往后。要打印所有的显式的栈帧。在kern/entry.s里可以找到一个简单的方式来得知什么时候停下。  
  
为了下面的练习和未来的实验，这里有一些具体的要点值得记忆。  
> + 如果int *p = (int*)100，则(int)p+1和(int)(p+1)是不同的值。前者为101而后者为104。当对一个指针增加一个整型值，这个整型会隐式地乘上这个指针指向数据类型地大小。  
> + p[i]等价于*(p+i),引用了p指向地内存地址后第i个对象。这里的加法服从上一条规律。
> + &p[i]等价于（p+i）,是对数组中第i个对象的取址。
  
尽管大部分C程序从不需要转换指针和整型数，操作系统却要频繁地这么做。无论何时看到设计内存地址的加法，要分清这是一个整型加还一个指针加，确认是否需要乘上数据类型大小。  
  
> + Exercise 11 
    按照上面地要求实现反向追踪函数，按照如上地格式输出，要不然打分脚本就不明白了。当认为它能够正确运行时，运行make grade指令看看它的输出是否符合打分脚本的期待，如果不行的话就修改它。在这之后可以以你喜欢的格式输出反向追踪函数。
    如果使用了read_ebp()，注意GCC可能会生成优化过的代码，在mon_backtrace（）的函数序码之前调用read_ebp(),这会导致一个不完全的栈追踪（最近的函数调用栈帧丢失了）。
    代码
    int
    mon_backtrace(int argc, char **argv, struct Trapframe *tf)
    {
	 // Your code here.
	 int regebp = read_ebp();
	 int *ebp = (int *)regebp;

	 cprintf("Stack backtrace:\n");
	
	 while ((int)ebp != 0x0) {
		cprintf("  ebp %08x", (int)ebp);
		cprintf("  eip %08x", *(ebp+1));
		cprintf("  args");
		cprintf(" %08x", *(ebp+2));
		cprintf(" %08x", *(ebp+3));
		cprintf(" %08x", *(ebp+4));
		cprintf(" %08x", *(ebp+5));
		cprintf(" %08x\n", *(ebp+6));

		ebp = (int *)(*ebp);
	  }
	 return 0;
    }
现在反向追踪函数可以给出使得mon_backtrace（）被执行的原调用函数在栈中的地址。然而，实际上一般需要知道和这个地址对应的函数名。例如，想要得知那个函数可能包含导致内核崩溃的bug时。  
  
为了实现这一函数功能，需要使用debuginfo_eip()函数，它在符号表中查阅eip，返回这一地址的debug信息。这个函数定义在kern/kdebug.c中。  
  
> + Exercise 12 
    修改栈反向追踪函数，显示每个eip的函数名，源文件名，行数。
    在debuginfo_eip中，__STAB_*类对象从何而来？这个问题的答案很长，为了发现答案，可以尝试下面的操作：
    在kern/kernel.ld中查找__STAB_*
    执行objdump -h obj/kern/kernel
    执行objdump -g obj/kern/kernel
    执行gcc -pipe -nostdinc -o2 -fno-builtin -I. -MD -Wall -Wno-format -DJOS_KERNEL -gstabs -c -s kern/init.c ，查看init.s
    检查启动加载器是否在在内存中加载了符号表，作为加载内核二进制码的一部分。
    插入stab_binsearch的调用来找到地址的行数，完成debuginfo_eip的实现。
    在内核监视器里加入反向追踪指令，拓展mon_backtrace实现，调用debuginfo_eip来为每一栈帧打印一行信息，以如下格式：
    K> backtrace
    Stack backtrace:
    ebp f010ff78  eip f01008ae  args 00000001 f010ff8c 00000000 f0110580 00000000
         kern/monitor.c:143: monitor+106
    ebp f010ffd8  eip f0100193  args 00000000 00001aac 00000660 00000000 00000000
         kern/init.c:49: i386_init+59
    ebp f010fff8  eip f010003d  args 00000000 00000000 0000ffff 10cf9a00 0000ffff
         kern/entry.S:70: <unknown>+0
    K> 
    每行信息给出文件名和栈帧在文件中的行数，接着是函数名和eip到函数第一个指令的偏移量。（例如，monitor+106意味着eip在monitor开始后106字节）
    确认打印文件和函数名在一个独立的行，以免打分脚本不能识别。
    提示：printf格式字符串提供了一个简单却又晦涩难懂的方法来打印非空终止的字符串，如STABS表里的那些。printf("%.*s",length,string)打印string中最多length长度的字符。查阅printf操作手册页来发现它是如何生效的。
    有些函数在方向追踪中丢失了，如有一个monitor()的调用却没有runcmd()的。这是因为编译器将一些函数调用优化到行内了。其他优化也许会导致看到期待以外的行数。如果在GNUmakefile中放弃-o2选项，反向追踪会更有意义，虽然内核会运行得更慢。

    插入stab_binsearch
    	stab_binsearch(stabs, &lline, &rline, N_SLINE, addr);
	if (lline > rline)
		return -1;
	info->eip_line = stabs[lline].n_desc;
    修改mon_backtrace
    	while ((int)ebp != 0x0) {
		cprintf("  ebp %08x", (int)ebp);
		cprintf("  eip %08x", *(ebp+1));
		cprintf("  args");
		cprintf(" %08x", *(ebp+2));
		cprintf(" %08x", *(ebp+3));
		cprintf(" %08x", *(ebp+4));
		cprintf(" %08x", *(ebp+5));
		cprintf(" %08x\n", *(ebp+6));

		//打印eip地址信息
		struct Eipdebuginfo info;
		debuginfo_eip(*(ebp+1), &info);
		cprintf("%s:%d: ", info.eip_file, info.eip_line);
		cprintf("%.*s+%d\n", info.eip_fn_namelen, info.eip_fn_name, *(ebp+1)-info.eip_fn_addr);
		
		ebp = (int *)(*ebp);
	}
    检查kedebug.c，__STAB_*类对象使用外部引用。检查kernel.ld链接脚本，与stab有关的部分为： 
	/* Include debugging information in kernel memory */
    	.stab : {
		PROVIDE(__STAB_BEGIN__ = .);
		*(.stab);
		PROVIDE(__STAB_END__ = .);
		BYTE(0)		/* Force the linker to allocate space
				   for this section */
	}

	.stabstr : {
		PROVIDE(__STABSTR_BEGIN__ = .);
		*(.stabstr);
		PROVIDE(__STABSTR_END__ = .);
		BYTE(0)		/* Force the linker to allocate space
				   for this section */
	}
    可知启动加载器在加载内核时为stb分配了内存。
    这里的PROVIDE使括号内元素在目标文件内被引用却未被声明时，使用该内存分区即.stab section/.stabstr scetion开头与结尾的记录的数据。这段分配内存的地址紧接在内核.text和.rodata分区内存之后。检查init.s，其中有.stab数据被写入的记录，即stab_binsearch涉及的部分。	`
  _
至此，grade 50/50，本实验完成。