# Lab 5 File system,Spawn and Shell
  
## 介绍
  
本实验将实现spawn，一个加载和运行硬盘可执行文件的库调用。然后丰富内核和库操作系统以能够在控制台上运行一个shell。这些功能需要一个文件系统，而这个使用介绍了一种简单的读写文件系统。  
  
本实验新的部分内容为文件系统环境，位于一个新的fs目录。浏览这个目录里的所有文件来感受都有哪些新的。同样地，在user,lib目录下也有和文件系统相关地新源文件。   
```
fs/fs.c 	操作文件系统硬盘上结构的代码。
fs/bc.c 	一个简单的区块cache，构建于用户等级页错误处理功能的顶部。
fs/ide.c 	小型的基于PIO(非中断驱动）IDE驱动代码。
fs/serv.c 	文件系统服务器，用文件系统IPC和客户环境交互。
lib/fd.c 	实现了通用类Unix文件描述符交互的的代码
lib/file.c 	硬盘上文件类型驱动，作为一个文件系统IPC客户端实现。
lib/console.c 	控制台输入输出文件类型驱动。
lib/spawn.c 	spawn库调用的代码框架。
```  
  
应该在将lab4的代码合并到新lab5代码后再次运行pingpong,primes和forktree测试程序。需要注释掉kern/init.c的ENV_CREATE(fs_fs)因为fs/fs.c尝试做一些I/O，而JOS还没允许。类似地，暂时注释掉lib/exit.c的close_all()的调用；这个函数调用了之后在实验中实现的子函数，因此调用的话会报错。如果lab4代码不包含任何bug，这个给测试条件应该会正常运行。不要在此之前继续。在开始做练习1时不要忘记去取消注释。  
  
如果不能正常运行，用`git diff lab4`来回顾所有的改变，确认lab4的代码没有在lab5里消失，可以正常工作。  
  
测试通过。  
  
## 文件系统预备
  
使用的一个文件系统会比包括xv6 Unix在内的“真实”的文件简单得多，但仍然足够强大来提供基础得功能：创建，读取，写入和删除组织在等级划分得目录结构文件。  
  
此时仅开发一个单用户操作系统，提供足够捕获bug得保护但不会在互相怀疑得用户间保护彼此。文件系统因此不支持UNIX得文件所有权和许可权得理论。同样也不支持硬链接，符号链接，时间戳，或者特殊驱动文件这类大多UNIX文件系统做的东西。  
  
### 盘上文件系统结构
  
大多UNIX文件系统把可用的硬盘空间划分为两个主要类型区域：节点区域和数据区域。UNIX文件系统为每个文件系统的文件分配一个节点；一i给文件节点保存文件关键的元数据如它的stat属性和它数据区块的指针。数据区域被分配大得多（通常8KB或更多）的数据区块，在此之内文件系统保存文件数据和目录元数据。目录记录包含文件名和节点的指针；一个硬链接文件是被多个文件系统目录记录引用的节点。因为本文件系统不支持硬链接，不需要这个等级的间接性，因此可以完成一个有效率的简化：文件系统将完全不使用节点，作为替代会简单地存储所有文件（或者子目录）的元数据到描述这个文件的唯一的目录记录。  
  
文件和目录逻辑上都由一系列的数据区块组成，分散在硬盘里，很像一个环境的虚地址空间页可以分散在物理地址。文件系统环境隐藏区块布局的细节，展示读写在文件特定偏移量的一系列字节的交互接口。文件系统环境内部处理所有的目录修改，作为执行如文件创建删除行为的一部分。文件系统允许用户环境直接读取目录元数据（例如用read)，这意味着用户环境自己执行问节目扫描操作（例如ls程序的实现）而不用依赖一个额外的文件系统调用。而这种目录扫描方法的弱点和大多现代UNIX版本不推荐它的理由是，它会使得应用程序依赖于目录元数据的格式，使得不通过改变或者至少重编译应用来改变文件内部布局变得很困难。  
  
#### 扇区和区块
  
大多磁盘不能以字节粒度执行读写，而只能以扇区为单位读写。在JOS里，扇区为512字节。文件系统实际以区块为单位分配和使用磁盘存储。小心区分这两个词：扇区大小是一个磁盘硬件的所有，而区块大小是操作系统使用磁盘的一个方式。一个文件系统区块大小必须是底层磁盘扇区大小的整数倍。  
  
UNIX xv6文件系统使用了一个512字节大小的区块，也是底层磁盘扇区大小。大多现代文件系统使用一个更大的区块大小，因为存储空间变得更廉价了而用大粒度管理存储会更有效率。本文件系统会使用4096字节的区块大小，方便对应处理器的页大小。  
  
#### 上位区块
  
文件系统通常保留特定磁盘区块到磁盘上“容易找到”的地方（例如最前或者最后）来存描述文件系统整体的属性的元数据。这些属性包括区块大小，磁盘大小，和找到根目录所需要的元数据，文件系统上一次加载的时间，上一次检错的时间和其他。这些特别的区块被称为上位区块。  
  
本文件系统将拥有一个上位区块，总是在磁盘的区块1上。它的布局定义在inc/fs.h的Super结构里。区块0通常被保留来存放启动加载器和隔离表，所以文件系统总的来说不会使用最前面的磁盘区块。很多“真实”的文件系统含有多个上位区块，复制在几个磁盘的大空间区域，使得如果其中之一损坏了或者磁盘在那个区域出现介质错误，其他的上位区块仍然能被找到和用来访问文件系统。  
  
#### 文件元数据
  
元数据的布局描述在inc/fs.c的File结构里。这个元数据包括文件名，大小，类型（常规文件或者目录），和包含文件区块的指针。就像之前提到的，本文件系统不使用节点，所以这个元数据存储在一个磁盘上的目录记录。不像大多“真实”文件系统，为了简化将使用一个File结构来代表文件元数据，使它同时出现在磁盘和内存里。  
![disk](https://user-images.githubusercontent.com/75117698/115263429-00fa3380-a168-11eb-87be-f7578429378c.png)
![file](https://user-images.githubusercontent.com/75117698/115263466-08214180-a168-11eb-94b9-00f0a92f7fb4.png)


  
File的f_direct数组包含存储文件头10个（NDIRECT)区块数的空间，称为文件的直接区块。对于至多10 * 4096 = 40KB大小的小文件，将直接放在File结构内部的区块数里。而对于更大的文件，需要一个地方来保存剩余的文件区块数。因此对于大于40KB的文件分配一个额外的磁盘区块，称为文件间接区块，存放多达4096/4 = 1024个额外区块数。文件系统因此可以允许文件使用1034个区块，或者刚刚超过4MB的大小。为了支持更大的文件，“真实的”文件系统通常还支持双重或者三重间接区块。  
  
#### 目录与常规文件
  
文件系统的一个File结构可以代表一个常规文件或者一个目录；这个两种类型的“文件”通过File结构的type字段区分。文件系统以同样的方式管理通常文件和目录文件，除了完全不会解释常规文件的数据区块内容，而解释目录文件内容为一些列描述目录内文件和子目录的File结构。  
  
文件系统的上位区块包含一个File结构（Super的root字段），保存这文件系统根目录的元数据。这个目录文件的内容是一系列描述位于文件系统根目录的文件和目录的File结构。所有根目录的子目录也可以再次包含File结构，作为一个子目录的子目录。  
  
## 文件系统
  
本实验的目标不是实现整个文件系统，但要实现一个特定的关键组件。具体的说，要负责完成读取区块到区块的cache里然后将它们刷回磁盘中；分配磁盘分区；映射文件偏移量到磁盘区块；在IPC接口中实现读写和打开。因为不会实现所有的文件系统，所有熟悉提供的代码和各种文件系统接口很重要。  
  
### 磁盘访问
  
操作系统的文件系统环境需要能够访问磁盘，但内核还没有实现任何的磁盘访问功能。作为传统“大而僵”操作系统思路（添加一个IDE磁盘驱动到内核，并添加必要的系统调用让文件系统来访问）的替代，将IDE磁盘驱动作为用户等级文件环境的一部分实现。仍然需要稍微修改内核，为了设置使得文件系统环境有独自实现磁盘访问的特权。  
  
用基于poll（programmed I/O)的磁盘访问可以简单地实现用户空间地磁盘访问而不用使用中断。在用户模式实现中断驱动地设备驱动也是可能的（如L3和L4做的那样），但因内核必须接受设备驱动然后分发它们到正确的用户模式环境而更难实现。  
  
x86处理器使用EFLAGS寄存器的IOPL位来决定保护模式代码允许执行特定的设备I/O指令如IN和OUT指令。因为所有需要的IDE磁盘寄存器放在x86的I/O空间而不是在内存映射的，给与文件系统环境I/O特权是为了允许文件系统访问这些寄存器唯一要做的。实际上，EFLAGS寄存器的IOPL位为内核提供了一个简单的“全或无”方法来控制用户模式代码是否能够访问I/O空间。在本实验的情况下，要使得文件系统环境有能力访问I/O空间，但也不能让其他环境有能力访问I/O空间。  
  
> + Exercise 1
    i386_init通过传递ENV_TYPE_FS类型到环境创建函数env_create来区分了文件系统环境。修改env.c的env_create，使得它能够给予文件系统环境I/O特权，但不会给其他的环境。  
	确保可以开启文件环境而不会引起一个通用保护错误，通过make grade的fs i/o测试。  
  
inc/mmu.h里找到Eflags寄存器标志位中I/O特权掩码为FL_IOPL_MASK。  
在env.c中env_create判读磁盘服务环境并设置特权：  
```
	if (type == ENV_TYPE_FS)
		e->env_tf.tf_eflags |= FL_IOPL_MASK;
```  
fs i/o 测试通过。  
  
> + Question
	1.是否有其他的方法来确保之后从一个环境切换到另一个时，这个I/O特权设置被正确地保存和恢复？  
  
进程切换过程中调用了env.pop_tf函数，这一步执行了寄存器恢复，避免了权限传递。  
  
注意本实验的GNUmakefile文件设置QEMU来实验boj/kern/kernel.img文件作为磁盘0的映射（通常就是DOS/Windows的C盘），然后实验新文件obj/fs/fs.img作为磁盘1（D盘）。本实验文件系统应该永远不碰磁盘1；磁盘0只被用来启动内核。如果操作破坏两个磁盘映射，可以重启两者到它们原本的版本，指令为：  
```
$ rm obj/kern/kernel.img obj/fs/fs.img
$ make
```
或者  
```
$ make clean
$ make
```
  
### 区块Cache
  
本文件系统中，将实现一个简单的"缓冲区 cache"(就是一个区块cache），通过处理器虚拟内存系统。区块cache的代码在fs/bc.c里。  
  
本文件系统将限制于处理3GB以内的硬盘。保留了一个大的固定的3GB文件系统环境地址空间区域，在0x10000000(DISKMAP)到0xD0000000(DISKMAP+DISKMAX)的空间，作为一个内存映射版的迎吧。例如，磁盘区块0被映射在虚拟地址0x10000000，磁盘分区1被映射在虚拟地址0x10001000，如此往后。fs/bc.c里的diskaddr函数实现了这个从磁盘数到虚拟地址的转换（附带一些健全性检查。  
  
因为文件系统环境有它自己的虚拟地址空间，独立于系统里其他环境的虚拟地址空间，文件系统环境需要做的至少实现文件访问，以这种方式保留大部分文件系统环境地址是有合理的。实现在32位机的真实文件系统来做这件事是不方便的，因为现代磁盘都大于3GB。这样的缓冲区cache管理方法在一个64位地址空间的机器上也许还是合理的。   
  
当然，将整个硬盘读取到内存里会花费很长时间，所以作为替代实现一个需求分页形式，只分配页到磁盘映射区域然后作为这个区域页错误的回应从磁盘中读取对应的区块。通过这种方式可以假装整个磁盘在内存里。  
  
> + Exercise 2
	实现fs/bc.c的bc_pgfault和flush_block函数。bc_pgfault是一个页错误处理函数，就像在前一个实验里写的写时拷贝fork，除了它是用从磁盘中加载页来回应页错误。在写这个函数时，记住：1.addr不能分配到区块边界。2.ide_read操作在扇区而不是区块上。
	flush_block函数应该在必要时写入一个区块到磁盘里。如果区块不在区块cache里（即页没有被映射）或者还不脏，flush_block不应该做任何事。使用VM硬件来保存追踪一个磁盘区块自它上次被读取或写入到磁盘后是否被修改，只要看uvpt记录里PTE_D脏位是否被设置。（作为写入到那一页的回应，PTE_D位被处理器设置；见386参考指南的[第五章]（http://pdos.csail.mit.edu/6.828/2011/readings/i386/s05_02.htm）5.2.4.3节。）在写入区块到磁盘后，flush_block应该用sys_page_map清除PTE_D位。  
	用make grade测试代码，现在应该通过check_bc，check_super和check_bitmap测试。  
  
fs/fs.c里的fs_init函数是如何使用区块cache的首个例子。在初始化区块cache后，它简单地存储指针到磁盘映射区域的super全局变量。在此之后，可以简单地从super结构里读取它们，就好像它们在内存中，而页错误处理函数在必要时从磁盘中读取它们。
  
bc_pgfault() 从磁盘加载页应对磁盘区块的页错误。  
检查错误是否在区块cache区域。检查区块数完整性。在磁盘映射区域里分配一页，从磁盘区块中读取内容到这个页。  
提示：首先对齐地址到页边界。fs/ide.c有读取磁盘的代码。  
从磁盘中读取区块后清除磁盘区块页的脏位。检查读取的页是否被映射。   
```
static void
bc_pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
	int r;

	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("page fault in FS: eip %08x, va %08x, err %04x",
		      utf->utf_eip, addr, utf->utf_err);

	if (super && blockno >= super->s_nblocks)
		panic("reading non-existent block %08x\n", blockno);

	addr = ROUNDDOWN(addr,PGSIZE);
	if ((r = sys_page_alloc(0,addr,PTE_U|PTE_P|PTE_W)) < 0)
		panic("bc_pgfault error,can not sys_page_alloc: %e ",r);
		
	if ((r = ide_read(blockno * BLKSECTS, addr, BLKSECTS)) < 0)
		panic("bc_pgfault error,can not ide_read: %e ",r);

	if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
		panic("in bc_pgfault, sys_page_map: %e", r);

	if (bitmap && block_is_free(blockno))
		panic("reading free block %08x\n", blockno);
}
```
flush_block() 必要时刷回包含va的区块内容到磁盘，然后用sys_page_map清除PTE_D位。如果区块不在区块cache或者非脏，则不做任何事。  
提示：使用va_is_mapped,va_is_dirty和ide_write。在调用sys_page_map时使用PTE_SYSCALL常量。不要忘记对齐地址。  
```
void
flush_block(void *addr)
{
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;

	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("flush_block of bad va %08x", addr);

	int r;
	 
	addr = ROUNDDOWN(addr,PGSIZE);
	if (va_is_mapped(addr) && va_is_dirty(addr))
	{
		if ((r = ide_write(blockno * BLKSECTS, addr, BLKSECTS)) < 0)
			panic("flush_block error, can not ide_write: %e ",r);
		if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
			panic("flush_block error, can not sys_page_map: %e ",r);
	}
}
```
  
check_bc，check_super和check_bitmap测试通过。  
  
### 区块位图
  
在fs_init设置了bitmap指针后，可以把bitmap当作一个包装过的位数组，每个代表磁盘上的一个区块。例如，block_is_free，简单的检查一个给定区块在位图中是否时标记位可用。  
  
> + Exercise3
	使用free_block作为实现fs/fs.c里alloc_block的的模型，找到位图里一个可用的磁盘区块，标记它为使用过的，并返回区块数。当分配一个区块，应该立即把改变过的位图区块刷回磁盘里 ，用flush_block，来维护文件系统的一致性。  
	使用make grade来测试代码。现在应该可以通过“alloc_block"。
  
alloc_block（）寻找bitmap里一个可用的区块并分配。  
提示：使用free_block作为操作位图的例子。  
bitmap由一个或多个区块构成。单个的bitmap区块包含了BLKBITSIZE个区块在使用的位。磁盘中总共有super->s_nblocks区块。
```
int
alloc_block(void)
{
	for (uint32_t blockno = 0; blockno < super->s_nblocks; ++blockno)
	{
		if (block_is_free(blockno))
		{
			bitmap[blockno/32] ^= 1<<(blockno%32);
			flush_block(bitmap);
			return blockno;
		}
	}
	return -E_NO_DISK;
}
```
alloc_block 测试通过。  
  
### 文件操作
  
现在在fs/fs.c里提供了一系列函数来实现基础功能，以支持解释和管理File结构，扫描和管理目录文件记录，从根走遍文件系统来完成一个绝对路径名。通读fs/fs.c里的所有代码，确保继续前自己理解每个函数做了什么。  
  
> + Exercise 4
	实现file_block_walk和file_get_block。file_block_walk从一个文件内的区块偏移量映射到文件结构里或间接区块中这个区块的指针，很像pgdir_walk为页表做的那样。file_get_block更进一步，映射到实际磁盘区块，必要时分配新的区块。  
	使用make grade测试代码。现在应该通过"file_open", "file_get_block", 和 "file_flush/file_truncated/file rewrite", 还有"testfile"的测试。  
  
fs.c中的函数：  
  
void check_super(void)：验证文件系统的上位区块。  
  
bool block_is_free(uint32_t blockno)：检查区块的位图是否提示blockno区块可用，可用时返回1，否则为0。  
  
void free_block(uint32_t blockno)：在位图中标记一个区块可用。  
  
int alloc_block(void)：搜索位图中一个可用的区块并分配它。分配后将改变的位图刷回磁盘。  
  
void check_bitmap(void)：验证文件系统位图。检查所有保留的区块为在使用。  
  
void fs_init(void)：初始化文件系统。  
  
static int file_block_walk(struct File \*f, uint32_t filebno, uint32_t \*\*ppdiskbno, bool alloc)：找到文件f中第filebno个区块磁盘区块数槽。设置\*ppdiskno指向那个槽。槽位是f->f_direct[]记录之一，或者间接区块的一个记录。  
当alloc设置时，函数会在必要时分配一个间接区块。  
返回为：  
0在成功时（但要注意\*ppdiskno也可能为0）。  
-E_NOT_FOUND在函数需要一个间接区块而alloc为0。  
-E_NO_DISK在硬盘上没有一个间接区块的空间时。  
-E_INVAL在fileno超出了范围（大于NDIRECT + NINDIRECT）时。  
类比：这就像pgdir_walk的文件版本。  
提示：不要忘记清除分配的区块。
```
static int
file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc)
{
       // LAB 5: Your code here.
	int ret;
	if (filebno >+ NINDIRECT + NDIRECT)
		return -E_INVAL;
	if (filebno < NDIRECT)
	{
		if (ppdiskbno)
			*ppdiskbno = f->f_direct + filebno;
		return 0;
	}	
	if (!alloc && !f->f_indirect)
		return -E_NOT_FOUND;
	if (!f->f_indirect)
	{
		if ((ret = alloc_block) < 0)
			return -E_NO_DISK;
		f->f_indirect = ret;
		memset(diskaddr(ret), 0, BLKSIZE);
		flush_block(diskaddr(ret));
	}
	if (ppdiskbno)
	   *ppdiskbno = (uint32_t*)diskaddr(f->f_indirect) + filebno - NDIRECT;
	return 0;
	   //panic("file_block_walk not implemented");
}
```  
file_get_block(struct File \*f, uint32_t filebno, char \*\*blk) 设置 \*blk到内存中文件f的第filebno个区块将被映射的地地址。  
成功时返回0，错误包括：  
-E_NO_DISK在区块需要被分配但磁盘已满时。  
-E_INVAL在filebno超出范围时。  
提示：使用file_block_walk和alloc_block。  
```
int
file_get_block(struct File *f, uint32_t filebno, char **blk)
{
	int ret;
	uint32_t **ppdiskbno;

	if ((ret = file_block_walk(f, filebno, ppdiskbno, 1)) < 0)
		return ret;
	if (*ppdiskbno == 0)
	{
		if ((ret = alloc_block()) < 0)
			return -E_NO_DISK;
		*ppdiskbno = ret;
		memset(diskaddr(ret),0,BLKSIZE);
		flush_block(diskaddr(ret));
	}
	*blk = diskaddr(*ppdiskbno);
	return 0;
}
```
static int dir_lookup(struct File \*dir, const char \*name, struct File \*\*file)    
尝试找到dir里名为name的文件，如果有，设置\*file到它。  
  
static int dir_alloc_file(struct File \*dir, struct File \*\*file)  
设置\*file指向dir里一个可用的文件结构，调用者负责填写文件字段。  
  
static const char\*  skip_slash(const char \*p)  以下划线跳过。  
  
static int walk_path(const char \*path, struct File \*\*pdir, struct File \*\*pf, char \*lastelem)  
评估一个路径名，从根开始。成功时设置\*pf到找到的文件，设置\*pdir到文件所在的目录。如果不呢找到文件但能找到它应该在的目录，设置\*pdir并拷贝最终路径元素到lastelm。  
  
int file_create(const char \*path, struct File \*\*pf)  
创建path，成功时设置\*pf指向这个文件并返回0.  
  
int file_open(const char \*path, struct File \*\*pf)  
打开path，成功时设置\*pf指向文件并返回0。  
  
ssize_t file_read(struct File \*f, void \*buf, size_t count, off_t offset)  
从f读取count字节到buf，从查询位置offset开始。这是对标准pread函数的模仿。返回读取的字节数。  
  
int file_write(struct File \*f, const void \*buf, size_t count, off_t offset)
从buf写入count字节到f里，从查询位置offset开始，这是对标准pwrite函数的模仿。必要时拓宽文件。返回写入的字节数。  
  
static int file_free_block(struct File \*f, uint32_t filebno)  
从文件f中移除一个区块。如果不在这里，什么都不做，成功时返回0。  
  
static void file_truncate_blocks(struct File \*f, off_t newsize)  
移除文件f当前使用的所有区块，但设置newsize时不必。对新和旧尺寸，计算要求的区块数，并从新区块数中清除旧区块数的区块。如果新区块数不比NDIRECT大，而间接区块已经被分配了（f->f_indirect !=0),然后释放间接区块。（记得清除f->f_indirec指针使得确认它是否有效）不改变f->f_size属性。  
  
int file_set_size(struct File \*f, off_t newsize) 设置文件f的尺寸，必要时截断或拓宽。  
  
void file_flush(struct File \*f)  刷回文件f的内容和元数据到磁盘。在文件的区块中循环。转换文件区块数到磁盘区块数并检查磁盘区块是否为脏。如果是则写出。  
  
void fs_sync(void)  同步整个文件系统。  
  
"file_open", "file_get_block", "file_flush/file_truncated/file rewrite", "testfile"的测试通过。  
  
### 文件系统接口
  
现在文件系统环境内部有了必要的功能，要让他变得可以被其他想要使用文件系统的环境访问。因为其环境不能直接调用文件系统环境的函数，将通过一种远程过程调用（RPC)来提供文件系统环境的访问。RPC建立在JOS的IPC机制上。调用文件系统服务的流程图如下（读取）：  

```  
      Regular env           FS env
   +---------------+   +---------------+
   |      read     |   |   file_read   |
   |   (lib/fd.c)  |   |   (fs/fs.c)   |
...|.......|.......|...|.......^.......|...............
   |       v       |   |       |       | RPC mechanism
   |  devfile_read |   |  serve_read   |
   |  (lib/file.c) |   |  (fs/serv.c)  |
   |       |       |   |       ^       |
   |       v       |   |       |       |
   |     fsipc     |   |     serve     |
   |  (lib/file.c) |   |  (fs/serv.c)  |
   |       |       |   |       ^       |
   |       v       |   |       |       |
   |   ipc_send    |   |   ipc_recv    |
   |       |       |   |       ^       |
   +-------|-------+   +-------|-------+
           |                   |
           +-------------------+
```
在点线之下的都是文件系统环境从常规环境获得一个读取请求的流程。从开始梳理，read（提供的）作用于所有文件描述符且简单地分发到正确地设备读取函数，在本例devfile_read(可以用更多地设备类型，如管道）。devfile_read实现了专用于盘上文件地read。这个和其他的lib/file.c的devfile_类函数实现了FS操作的客户端，所有的工作都以这种方式调用，在请求结构里集成参数，调用fsipc来发送IPC请求，然后解包返回结果。fsipc函数简单地处理了发送请求到服务器和接受回复的通用细节。  
  
文件系统服务器代码可以在fs/serv.c里找到。它循环在serve函数里，无尽地通过IPC接受请求，分发请求到serve_read。后者负责IPC读取请求的具体细节如解包请求结构和最终调用file_read来实际地执行文件读取。  
  
回想JOS的IPC机制，让一个环境发送一个单一的32位数字，可选地共享一个页。为了从客户端发送请求到服务器，使用32位数字来表示请求类型（文件系统服务器的RPC都是数字化的，就像系统调用那样）并存储请求的参数到一个union Fsipc在IPC的共享页。在客户端侧，总是能够分享页在fsipcbuf；在服务器侧，映射将要到来的请求页到fsreq(0x0ffff000)。  
  
服务器也通过IPC返回回应。使用32位数字表示函数的返回码。对大多数RPC，这就是返回的所有。FSREQ_READ和FSREQ_STAT也返回数据，通过简单地写入到客户端发送它的请求的页上。不需要发送这个页作为IPC的回应，因为客户端已经共享了这个页给服务器。同样的，在它的回应中，FSREQ_OPEN共享了一个新Fd页给客户端。现在来稍微处理文件描述符页。  
  
> + Exercise5
	实现fs/serv.c的serve_read。
	serve_read的重头已经在实现了的fs/fs.c函数file_read里完成了（事实上是一堆file_get_block的调用）。serve_read只要为文件读取提供RPC接口。阅读serve_set_size的代码和注释来获取服务器函数应该被怎么架构的大体概念。  
	使用make grade测试代码，现在应该通过"serve_open/file_stat/file_close"和"file_read"测试，得分为70/150。
  
serve_read()  从ipc->read.req_fileid的当前查询位置读取最多ipcipc->read.req_n字节的数据。返回给调用者从文件读取的字节数，并更新查询位置。
```
int
serve_read(envid_t envid, union Fsipc *ipc)
{
	struct Fsreq_read *req = &ipc->read;
	struct Fsret_read *ret = &ipc->readRet;

	if (debug)
		cprintf("serve_read %08x %08x %08x\n", envid, req->req_fileid, req->req_n);

	// Lab 5: Your code here:
	struct OpenFile *opf;
	int r,reqn;
	//查找文件
	if ((r = openfile_lookup(envid, req->req_fileid, opf)) < 0)
		return r;
	reqn = (req->req_n > PGSIZE ? PGSIZE:req->req_n);
	//读取文件
	if ((r = file_read(opf->o_file, ret->ret_buf, reqn, opf->o_fd->fd_offset)) < 0)
		return r;
	//偏移量修正
	opf->o_fd->fd_offset += r;
	return r;
}
```
"serve_open/file_stat/file_close"和"file_read"测试通过，得分70/150。  
  
> + Exercise6
	实现fs/serv.c的serve_write和lib/file.c的devfile_write。
	使用make grade测试代码，现在应该通过 "file_write", "file_read after file_write", "open", 和"large file"测试，得分为90/150。  
  
serve_write()  从req->req_buf写入req->req_n到req_fileid，从当前的查询位置开始，并以此更新查询位置。 必要时拓宽文件。返回写入的字节数。 
```
int
serve_write(envid_t envid, struct Fsreq_write *req)
{
	if (debug)
		cprintf("serve_write %08x %08x %08x\n", envid, req->req_fileid, req->req_n);

	struct OpenFile *opf;
	int r,reqn;
	//查找文件
	if ((r = openfile_lookup(envid, req->req_fileid, &opf)) < 0)
		return r;
	reqn = (req->req_n > PGSIZE ? PGSIZE:req->req_n);
	//写入文件
	if ((r = file_write(opf->o_file, req->req_buf, reqn, opf->o_fd->fd_offset)) < 0)
		return r;
	//偏移量修正
	opf->o_fd->fd_offset += r;
	return r;
}
``` 
devfile_write()  从buf写入至多n字节到fd当前的查询位置。成功时返回写入的字节数。  
制作一个FSREQ_WRITE请求给文件系统服务器。注意：fsipcbuf.write.req_buf只有这么大，但记住写入永远允许写入比要求更少的字节。
```
static ssize_t
devfile_write(struct Fd *fd, const void *buf, size_t n)
{
	int r;
	//写入字节数截断为小于buf容量
	n = (n > sizeof(fsipcbuf.write.req_buf) ? sizeof(fsipcbuf.write.req_buf) : n);
	fsipcbuf.write.req_fileid = fd->fd_file.id;
	fsipcbuf.write.req_n = n;	
	memmove(fsipcbuf.write.req_buf, buf, n);
	r = fsipc(FSREQ_WRITE, NULL);
	return r;
}
```
"serve_open/file_stat/file_close"和"file_read"测试通过，得分90/150。

  
## 进程衍生
  
已经提供了spawn的代码（在lib/spawn.c）里，它创建一个新环境，从文件系统里加载一个程序镜像，然后在子环境中运行程序。父环境然后继续运行，独立于子环境。spawn函数实现表现得就像UNIX里得fork紧接着一个子进程里执行以一个exec。  
  
实现spawn而不是一个UNIX风格得exec是因为spawn更容易从用户空间以外核风格实现，而不用内核的协助。思考为了实现用户空间的exec需要做什么，确保理解为什么它更难。  
  
> + Exercise7
	spawn依赖于新系统调用sys_env_set_trapframe来初始化新建的环境状态。实现kern/syscall.c里的kern/syscall.c。（不要忘记在syscall()分发新系统调用)。
	测试代码，从kern/init.c运行user/spawnhello，它尝试从文件系统spawn /hello。
	也用make grade 测试。  
  
sys_env_set_trapframe（） 设置envid的陷入帧到tf。tf被修改来确认用户环境总是运行在代码保护等级3，中断使能，且IOPL为0。  
成功时返回0，错误包括：-E_BAD_ENV在环境envid当前不存在，或者调用者没有修改envid的许可时。    
记得检查用户是否提供了一个正确的地址。  
```
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	struct Env *e;
	int r;

	if (r = envid2env(envid, &e, 1) < 0)
		return -E_BAD_ENV;
	user_mem_assert(e, tf, sizeof(struct Trapframe), PTE_U);
	//设置陷入帧
	e->env_tf = *tf;
	// 设置保护等级3
	e->env_tf.tf_cs |= 3;
	// IOPL置0
	e->env_tf.tf_eflags &= ~FL_IOPL_3;
	// 中断使能
	e->env_tf.tf_eflags |= FL_IF;
	return 0;
}
```
  
syscall分发调用，添加case:  
```
		case (SYS_env_set_trapframe):
			return sys_env_set_trapframe(a1,(struct Trapframe*)a2);
```
  
init.c里设置调用spawnhello:  
`	ENV_CREATE(user_spawnhello, ENV_TYPE_USER);`  
得到输出：  
```
enabled interrupts: 1 2 4
FS is running
FS can do I/O
Device 1 presence: 1
i am parent environment 00001001
block cache is good
superblock is good
bitmap is good
alloc_block is good
file_open is good
file_get_block is good
file_flush is good
file_truncate is good
file rewrite is good
hello, world
i am environment 00001002
No runnable environments in the system!
```
### 跨fork和spawn共享库状态
  
UNXI文件描述符是一个通用概念涵盖了管道，控制台I/O和其他。在JOS里，每个这样的设备类型都有一个对应的Dev结构，还有指向实现每个设备读写之类的函数的指针。lib/fd.c实现了通用类UNIX文件描述符接口。每个Fd结构指示了它的设备类型，大多lib/fd.c的函数简单地分发了操作到Dev结构里正确地函数。  
  
lib/fd.c也包含文件描述符表区域，在每个应用环境地地址空间，从FDTABLE开始。这个区域保留了一页大小（4KB）的地址空间，多达MAXFD(现在是32）个文件描述符，是每个应用一次可以打开的数目。无论何时，一个特定的文件描述符表页只在对应的文件描述符使用时映射。每个文件描述符也有一个可选的数据页，在FILEDATA开始的区域里，供设备使用。  
  
想要跨fork和spawn来共享文件描述符状态，但文件描述符状态被保存在用户空间内存。目前，对应fork，内存被标记为写时拷贝，所以状态会被复制而不是共享。（这意味着环境不会有能力来查找不是它们打开的文件，管道也不能跨fork工作）对于spawn，内存会被留下，完全不拷贝。（事实上，被spawn的环境开始时没有任何打开的文件描述符。）
  
现在改变fork来了解当前区域的内存被库操作系统使用并应该永远被共享。不用在某处硬编码一系列的区域，而设置页表记录中另一个没使用的位。（就像fork里使用PTE_COW)。
  
在inc/lib.h里已经定义了一个新PTE_SHARE位。在Intel和AMD操作手册里，这个位是三个PTE位的之一，代表可被软件使用获得。定义这样的惯例，如果一个页表记录的这个位被设置了，PTE应该直接从父环境拷贝到子环境，fork和spawn都是。注意这个不同于标记它为写时拷贝：就像第一代描述的，要确保共享更新到页。  
  
> + Exercise8
	改变lib/fork.c的duppage来遵循新的惯例。如果页表记录的PTE_SHARE位被设置，就直接拷贝映射。（应该使用PTE_SYSCALL，而不是0xfff来掩盖掉相关的标志位。0xfff页被用来表示访问过的和脏的标志位。）
	同样的，实现lib/spawn.c的copy_shared_pages 。它应该循环当前进程的整个页表记录（就像fork一样），拷贝所以有PTE_SHARE标志位的页映射到子进程。
  
make run-testpteshare来检查代码运行。应该看到"fork handles PTE_SHARE right" 和"spawn handles PTE_SHARE right"。  
  
make run-testfdsharing检查文件描述符是否正确共享。应该看到 "read in child succeeded"和"read in parent succeeded"。  
  
duppage添加新的判断：  
```
	if (pte & PTE_SHARE)
	{
		if ((r = sys_page_map(thisenv->env_id, addr, envid, addr, perm & PTE_SYSCALL)) < 0)
		{	panic("duppage: page remapping failed %e", r);
			return r;
		}	
	}
```
copy_shared_pages()  拷贝共享页映射到子地址空间。  
```
static int
copy_shared_pages(envid_t child)
{
	// LAB 5: Your code here.
	int i, j, pn, r;
	// 遍历用户空间映射页目录
	for (i = PDX(UTEXT); i < PDX(UXSTACKTOP); i++) {
		// 存在的页目录记录		
		if (uvpd[i] & PTE_P) {
			// 遍历该记录中的页表
			for (j = 0; j < NPTENTRIES; j++) {
				// 获取页地址
				pn = PGNUM(PGADDR(i, j, 0));
				// 到达栈顶页退出遍历
				if (pn == PGNUM(UXSTACKTOP - PGSIZE))
					break;
				// 映射共享页
				if ((uvpt[pn] & PTE_P) && (uvpt[pn] & PTE_SHARE)) {
					if ((r = sys_page_map(0, (void *)PGADDR(i, j, 0), child, (void *)PGADDR(i, j, 0), uvpt[pn] & PTE_SYSCALL)) < 0)
						return r;
				}
			}
		}
	}
	return 0;	
}
```  
运行testpteshare得到输出：  
```
enabled interrupts: 1 2 4
FS is running
FS can do I/O
Device 1 presence: 1
block cache is good
superblock is good
bitmap is good
alloc_block is good
file_open is good
fork handles PTE_SHARE right
file_get_block is good
file_flush is good
file_truncate is good
file rewrite is good
spawn handles PTE_SHARE right
Welcome to the JOS kernel monitor!
```
运行testfdsharing得到输出：  
```
enabled interrupts: 1 2 4
FS is running
FS can do I/O
Device 1 presence: 1
block cache is good
superblock is good
bitmap is good
alloc_block is good
file_open is good
file_get_block is good
file_flush is good
file_truncate is good
file rewrite is good
going to read in child (might page fault if your sharing is buggy)
read in child succeeded
read in parent succeeded
Welcome to the JOS kernel monitor!

```
  
## 键盘中断
  
为了使shell能够工作，需要一个向其输入的方法。QEMU已经展示了写入的输出到CGA显示和串行端口，但至此以后只在内核监控器里输入。在QEMU里，输入键入到图形窗口表现为键盘输入到JOS，而输入键入到控制台表现为串行端口上的字符。kern/console.c已经包含了键盘和串口驱动，自lab1以来就一直被内核监控器使用。但现在要把它们附加到系统其他的地方。  
  
> + Exerciese 9
	在kern/trap.c调用kbd_intr来处理陷入IRQ_OFFSET+IRQ_KBD；调用serial_intr处理陷入IRQ_OFFSET+IRQ_SERIAL。  
  
实验已经实现了控制台输入输出文件键入，在lib/console.c里。kbd_intr和serial_intr用最近读取的输入填充缓冲区而控制台文件键入抽出缓冲区（控制台文件键入默认被用来stdin/stdout除非用户重定向了它们）。  
  
测试代码，运行make run-testkbd，并输入几行。这个系统应该回复输入的行在你完成它们时。尝试在控制台和图形窗口上测试。  
  
运行make run-testkbd，输入行，得到输出：  
```
Type a line: sss
sss
Type a line: asd
asd
Type a line: qqq
qqq
Type a line: 
```  
  
## Shell
  
运行make run-icode或者make run-icode-nox。这会运行内核并开始user/icode. icode执行初始化，将设置控制台作为文件描述符0和1（标准输入输出）。它然后会spawn sh环境。现在可以运行下列指令：  
	echo hello world | cat
	cat lorem |cat
	cat lorem |num
	cat lorem |num |num |num |num |num
	lsfd
  
注意用户库方法cprintf直接向控制台打印，而不使用文件描述符代码。这对于debug很有帮助，但不利于从管道传输给别的程序。为了打印输出到特定的文件描述符（例如，1，标准输出），实验fprintf(1, "...", ...)。printf("...", ...）是打印到FD 1的快捷指令。例子见user/lsfd.c。  
  
> + Exercise10
	shell不支持I/O重定向，用sh < script代替手打所有指令到脚本是很棒的。添加I/O重定向<到user/sh.c。
	测试代码，输入sh < script到shell。  
	运行make run-testshell来测试shell。testshell简单地反馈了上述指令（也可以在fs/testshell.sh找到）到shell然后检查输出和fs/testshell.key地匹配。  
  
shell的 case '<'  输入重定向。从参数列表中抓取文件名。打开t做文件描述符0来读取（环境用来作为输入）。不能打开文件到一个指定的描述符，所有作为fd打开文件，然后检查fd是否为0，如果不是，将fd复制（dup）到文件描述符0，然后关闭原来的fd。  
```
		case '<':	// Input redirection
			// Grab the filename from the argument list
			if (gettoken(0, &t) != 'w') {
				cprintf("syntax error: < not followed by word\n");
				exit();
			}
			// LAB 5: Your code here.
			// 只读打开
			if ((fd = open(t, O_RDONLY)) < 0) {
				cprintf("open %s for read: %e", t, fd);
				exit();
			}
			if (fd) {
				dup(fd, 0);
				close(fd);
			}			
			//panic("< redirection not implemented");
			break;
```
  
运行make run-testshell得到输出：  
```
running sh -x < testshell.sh | cat
shell ran correctly
```
  
make grade 得分150/15。 
  
至此，本实验完成。
