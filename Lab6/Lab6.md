# Lab 6 Network Driver
  
## 介绍
   
这个实验为默认的最终项目。  
  
现在拥有文件系统，而成熟的OS离不开网路栈。在这个实验里将写一个网络接口卡驱动。这个网卡就基于Intel 82450EM芯片，也被称为E1000。  
  
然而，本实验的网卡驱动并不足够使得OS连到互联网。在本实验的新代码里，已经提供了一个网络栈和一个网络服务器。检查新目录net/和kern/的新文件。  
  
除了写驱动外，需要创建一个系统调用接口来访问驱动。实现网络服务器代码中缺失的部分来传递网络栈和驱动直接的包。通过完成一个网页服务器来将所有东西连在一起。有新网页服务器后就可以从文件系统里服务里提供文件服务了。  
  
内核设备驱动代码有很多需要自己完成的部分。本实验相比之前的实验提供了更少的指导：没有框架玩家，没有写好的系统调用接口，很多设计决定留给自己完成。因此，推荐阅读整个作业笔记，在实际开始个人练习前。  
  
## QEMU的虚拟网络
  
使用QEMU的用户模式网络栈，它不要求任何的管理员权限来运行。QEMU的文档有更多关于用户网络的部分，见[这里](http://wiki.qemu.org/download/qemu-doc.html#Using-the-user-mode-network-stack)。实验已经更新了makefile来使能QEMU的用户模式网络栈和虚拟E1000网卡。  
  
默认地，QEMU提供了一个虚拟路径运行在IP 10.0.2.2并分配JOS的IP地址为10.0.2.15。为了让事情简单，实验将这些默认设置硬编码到net/ns.h的网络服务器里。  
  
当QEMU的虚拟网络允许JOS来伪造网络连接，JOS的10.0.2.15地址在QEMU内部运行的虚拟网络之外没有任何意义（QEMU表现得就像一个NAT），所以不能直接连接到JOS内部的服务器，即使从运行QEMU的主机上。为了处理这点，实验设置了QEMU在某个主机上的端口上来运行一个服务器，使得可以简单地连接到这个JOS里的端口，在真实主机和虚拟网络之间传入传出数据。  
  
运行JOS服务器，在端口7(echo)和80（http)。运行make which-ports来查明QEMU将前往哪个开发主机端口。为了方便，makefile还提供了make nc-7和make nc-80来允许你直接和运行在终端的这些端口交互。（这些目标只连接到一个运行着的QEMU实例；必须另外开启一个QEMU。）  
  
### 数据包检查  
  
makefile设置了QEMU的网络栈来记录所有将要到来和输出的数据包，在lab目录的qemu.pcap里。  
  
用下面的指令来获得一个十六进制/ASCII抓取包的dump：  
`tcpdump -XXnr qemu.pcap`  
也可以使用[Wireshark]（http://www.wireshark.org） 来图形化地检查pcap文件。Wireshark特只读如何解码和检查数百种网络协议。  
  
### E1000调试
  
能使用模拟硬件是很幸运的。因为E1000运行在程序里，模拟的E1000可以用一种用户可读的格式向我们报告，它的网络状态和它遭遇的所有问题。通常，这对于在裸机上的驱动开发人员是难以获得的奢侈。  
  
E1000可以产生很多debug输出，所有要开启具体的日志频道。有用的频道有：  

|   标志位	|  意义							|
|  ---- 	|  ---- 							|
| tx		| 记录数据包传输操作 |
| txerr		| 记录传输环错误 |
| rx		| 记录RCTL的改变 |
| rxfilter	| 记录传入数据包的过滤 |
| rxerr		| 记录接收的环错误 |
| unknown	| 记录未知寄存器的读取和写入 |
| eeprom	| 记录从EEPROM的读取 |
| interrupt	| 记录中断寄存器的中断和改变 |  
  
使用`make E1000_DEBUG=tx,txerr...`的指令来开启记录。  
  
> + 注意：E1000_DEBUG标志位只在6.828版本的QEMU上有效。  
  
可以使用软件模拟硬件来单步调试。如果出现了不能理解为何E1000不能如预想的那样运行，可以看看hw/net/e1000.c的QEMU E1000实现。
  
## 网络服务器
  
从零开始写一个网络栈是很困难的。作为替代，使用使用了lwIP，一个开源轻量TCP/IP协议套件，涵盖了很多东西，包括一个网络栈。可以在[这里](https://savannah.nongnu.org/projects/lwip/)找到更多关于lwIP的东西。在本实验中，lwIP是一个实现了BSD套接字接口的黑盒子，有一个数据包输入端口和一个数据包输出端口。  
  
网络服务器实际上是4个环境的组合：  
> + 核心网络服务器环境（包括套接字调用解包器和lwIP）  
> + 输入环境
> + 输出环境
> + 计时器环境
  
下图展示了不同的环境和它们的关系。图展示整个系统包括设备驱动，之后会提到。本实验里需要完成的部分高亮为绿色。  
  
  
### 核心网络服务器环境
  
核心网络服务器环境由套接字调用解包器和lwIP自身组成。套接字调用解包器就像文件服务器一样工作。用户环境使用stubs(在lib/nsipc.c里）来发送IPC消息到核心网络环境。如果看lib/nsipc.c会发现用构筑文件服务器的方式构筑了核心网络服务器：i386_init用NS_TYPE_NS来创建NS环境，所有可以扫描envs查找整个特殊的环境类型。对每个用户环境IPC，网络服务器的解包器代表用户调用lwIP提供的正确的BSD套接字接口函数。  
  
常规用户环境不直接使用nsipc_类调用。作为替代，它们使用lib/sockets.c里的函数，其中提供了一个基于文件描述符的套接字API。因此，用户环境通过文件描述符引用套接字，就像引用盘上文件一样。很多操作（connect,accept...)都是套接字专用的，但read,write,close在普通文件描述符设备分发代码（lib/fd.c)里也经常使用。很像文件服务器维持所有打开文件内部唯一的ID，lwIP也为所有打开的套接字生成唯一ID。在文件服务器和网络服务器里，使用Fd结构存储的信息来映射每个环境的文件描述符到这些唯一ID空间。  
  
即使文件服务器的IPC解包函数和网络服务器看起表现得一样，仍有一个关键得不同。BSD套接字调用像accept和recv可以无限地阻塞。如果解包器让lwIP执行这些阻塞调用中的一个，解包器也会阻塞，整个系统可能一时间之一一个活跃的网络调用。因为这是不可接受的，网络服务器使用用户等级线程来避免阻塞整个服务器环境。对每个将要到来的IPC消息，解包器创建一个线程并在新线程里处理请求。如果线程阻塞，就只有那个线程在睡眠状态而其他线程继续运行。  
  
作为核心网络环境的补充，这里由三个助手环境。除了从用户应用接受消息，网络环境的解包器也从输入和计时器环境接受消息。  
  
### 输出环境
  
当服务于用户环境套接字调用，lwIP为网卡生成数据包来传输。lwIP会发送每个数据包传输到输出注释环境，使用NSREQ_OUTPUT IPC消息，将数据包附在IPC消息的页参数上。输出环境复制解说这类消息，并通过系统调用接口把数据包推向设备驱动。  
  
### 输入环境
  
被网卡接受的数据包需要被注入到lwIP。对每个设备驱动接收的数据包，输入环境从内核空间拉去数据包（使用内核系统调用）并发送这些数据包到核心服务器环境，使用NSREQ_INPUT IPC消息。  
  
数据包输入功能从核心网络环境中分离出来是因为JOS使得持续接受IPC消息和从设备驱动poll/wait一个数据包变得很难。JOS没有有select系统调用，允许环境来监控多个输入源来确认哪个输出已经准备好被处理。    
  
如果看看net/input.c和net/output.c会看到都需要被实现。这个主要因为实现依赖于系统调用接口。在实现了驱动和系统调用接口后完成它。  
  
### 计时器环境
  
计时器环境定期地发送NSREQ_TIMER消息到核心网络服务器，提示它一个计时器已经到期。这个线程地计时器消息被lwIP用来实现多个网络计时。  
  
## Part A 初始化和传输数据包
  
内核还没有时间概念，所以要添加。硬件每10ms生成一个临时地时钟中断。每个时钟中断上都可增加一个变量来提示时间已经前进了10ms。这在kern/time.c里实现，但还没有完全集成在内核里。  
  
> + Exercise1
	为kern/trap.c地每个时钟中断添加一个time_tick调用。实现sys_time_msec并添加它到kern/syscall.c的syscall里，使得用户空间可以访问时间。  
  
使用`make INIT_CFLAGS=-DTEST_NO_NS run-testtime`来测试代码。应该看到环境5次的每秒倒计时。-DTEST_NO_NS关闭了网络服务器环境的开始，因为此时它会报错。  
  
trap.c里时钟中断添加tick调用。小心，多处理器里时钟信号被每个CPU触发。
```
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
                lapic_eoi();
				time_tick();
                sched_yield();
                return;
 	}
```
  
sys_time_msec() 返回当前的时间：  
```
static int
sys_time_msec(void)
{
	return time_msec();
}
```  
syscall中分发系统调用：
```
		case (SYS_time_msec):
			return sys_time_msec();
```
	
运行`make INIT_CFLAGS=-DTEST_NO_NS run-testtime`，得到输出：  
`starting count down: 5 4 3 2 1 0 `  
  
### 网络接口卡
  
写一个驱动要求对硬件和提供给软件的接口有很深的理解。实验文本提供了如何和E1000交互的一个高等级的概述，但在写驱动时需要大量使用Intel操作手册。  
  
> + Exercise 2
	浏览Intel的[软件开发者操作手册](https://pdos.csail.mit.edu/6.828/2018/readings/hardware/8254x_GBe_SDM.pdf)中E1000的部分。这个操作手册涵盖了几个和以太网紧密相关的控制器。QEMU模拟了82540EM。
	速览第二章来感受设备的部分。写驱动需要熟悉章节3和14，还有4.1（而不是4.1的小节）。还需要使用章节13作为参考。其他章节主要包含了E1000中驱动不会与之交互的部分。现在不用担心细节；只要感受文档是怎么架构的，好在之后可以找到。
	当阅读操作手册，记住E1000是一个有着先进功能的精密的设备。一个运行着的E1000驱动只需要NIC提供的一小部分功能和交互。仔细想想和网卡交互的最简单的方法。强烈推荐在利用先进功能前让一个基础驱动工作。  
  
#### PCI接口

E1000是一个PCI设备，意味着它插入到主板的PCI总线里。PCI总线有地址，数据和中断行，允许CPU和PCI设备通信，PCI设备读取和写入内存。一个PCI设备需要在可以使用前被发现和初始化。遍历PCI总线来寻找连接的设备。初始化时分配I/O和内存空间还有协定设备使用的IRQ行。  
  
kern/pci.c里已经提供了PCI代码。为了在启动阶段执行PCI的初始化，PCI代码遍历PCI总线来查找设备。当代码找到一个设备，它读取设备的厂商ID和设备ID并以这两个值作为键值搜索pci_attach_vendor数组。这个数组用pci_driver记录组成，其结构如下：  
```
struct pci_driver {
    uint32_t key1, key2;
    int (*attachfn) (struct pci_func *pcif);
};
```
  
如果发现了设备的厂商ID和设备ID匹配数组的记录，PCI代码调用记录的attachfn来执行设备初始化。（设备可以被类确认，这就是kern/pci.c里其他驱动表的意义。）  
  
attach函数传递一个PCI函数来初始化。PCI卡可以提供多个函数，尽管E1000只提供了一个。在JOS里描述一个PCI函数的方法如下：  
```
struct pci_func {
    struct pci_bus *bus;

    uint32_t dev;
    uint32_t func;

    uint32_t dev_id;
    uint32_t dev_class;

    uint32_t reg_base[6];
    uint32_t reg_size[6];
    uint8_t irq_line;
};
```
上面的结构反应了一些开发者手册里4.1节的表4-1的记录。本实验需要关心最后三个oci_func结构记录，因为它们记录了设备的协商内存，I/O，和中断源。 reg_base和reg_size数组包含六个基础地址寄存器（BAR)的信息。reg_base为内存映射I/O区域（或者I/O端口资源的基础I/O端口）存储了基础内存地址，reg_size包含了字节大小或者reg_base的对应基础值的I/O端口数，irq_line包含了设备中断分配的IRQ行。E1000的BAR具体含义在表4-2的下半段。  
  
当设备的附属函数被调用了，驱动已经被找到但还没有使能。这意味着PCI代码还没决定分配给设备的资源，如地址空间和一个IRQ行，因此，pci_func结构的最后三个元素还没被填入。附属函数应该调用pci_func_enable,它会使能设备，协商这些资源，然后填到pci_func结构里。  
  
> + Exercise3
	实现一个附属函数来初始化E1000。添加一个记录到kern/pic.c的pci_attach_vendor数组来触发函数，在一个匹配的设备被发现时（确保把它放在（0，0，0）记录之前，它标记了表的结束）。可以使用QEMU模拟的82540EM的厂商ID和设备ID，见5.2小节。应该看看PCI总线启动时JOS扫描列出的东西。  
	现在，通过pci_func_enable使能设备。本实验还会添加更多的初始化。
	本实验提供了kern/e1000.c和kern/e1000.h文件来避免搞乱构建系统。它们目前是空白的；在本练习里完成它们。需要在内核其他包含e1000.h文件。
	启动内核时，应该看到它打印了E1000卡的PCI函数被使能。代码现在应该通过pci attach测试。  
  
查找手册中82540EM的id记录，为厂商ID： 8086h；设备ID：100E,在pcireg.h里定义：  
```
#define VENDOR_ID_82540EM 0x8086
#define DEVICE_ID_82540EM 0x100E
```
  
e1000.h里声明初始化函数，并在e1000.c中定义，通过pci_func_enable使能设备。  
```
int e1000_init(struct pci_func *pcif){
    pci_func_enable(&pcif);
    return  1;
}
```  
pci_attach_vendor数组记录添加：  
```
struct pci_driver pci_attach_vendor[] = {
	{  0x8086, 0x100E, &e1000_init  },
	{ 0, 0, 0 },
};
```
通过pci attach测试。  
  
#### 内存映射I/O
  
软件通过内存映射I/O（MMIO）。之前在JOS里见过两次：CGA控制台和LAPIC都是通过读写内存来控制和查询的设备。但读写不会到DRAM；它们直接去到各自的设备。  
  
pci_func_enable和E1000协商了一个MMIO区域，并存储它的基址和大小到BAR 0（即reg_base[0] 和reg_size[0]）。这是一段分配给设备的物理内存地址，意味着需要通过虚拟地址来做一些事情访问它。因为MMIO区域分配在很高的物理地址（通常在3GB之上），受制于JOS的256MB限制，不能使用KADDR来访问。因此要创建一个新内存映射。使用MMIOBASE之上的区域（lab4的mmio_map_region会确保我们不会覆盖LAPIC使用的映射）。因为PCI设备初始化发生在JOS创建用户环境之前，可以在kern_pgdir里创建映射，它就能一直可访问。  
  
> + Exercise4
	在附属函数中为E1000的BAR0创建一个虚拟内存映射，通过调用mmio_map_region（lab4里写的，用来支持LAPIC的内存映射）。
	要记录这个映射区域到一个变量里，好在之后可以访问映射的寄存器。看看kern/lapic.c的lapic变量作为一个例子。如果使用了一个指针来映射设备寄存器，确保声明它为volatile；否则编译器会允许在这个内存中缓存值和重新排序访问。
	尝试着打印处设备状态寄存器（13.4.2节）来测试映射。这是一个4字节寄存器，从寄存器空间的第8字节开始。另外，应该获得0x80080783，它指示了全双工链路速度高达1000MB/s。
  
提示：需要很多常量，像寄存器位置和位掩码的值。尝试从开发者手册中拷贝出这些东西时易错的，且会导致痛苦的除错环节。实验推荐使用QEMU的[e1000_hw.h](https://pdos.csail.mit.edu/6.828/2018/labs/lab6/e1000_hw.h)头文件作为向导。不推荐逐字拷贝它，因为它定义了很多实际不需要使用的东西，也可能没有以需要的方式定义，但确实是个好切入点。  
  
e1000.h里添加宏和定义：    
```
#define E1000REG(offset) *(e1000 + offset/4)
#define DSTATUS   0x00008   
```
定义易变指针变量e1000，记录MMIO区域，打印状态信息：  
```
volatile uint32_t* e1000;

int e1000_init(struct pci_func *pcif){
    pci_func_enable(pcif);
    e1000 = mmio_map_region(pcif->reg_base[0],pcif->reg_size[0]);
    printf("e1000: status %x\n", E1000REG(DSTATUS));
    return  0;
}
```
  
启动JOS，得到输出：  
`e1000: status 80080783`  
  
### DMA
  
可以用读写E1000的寄存器想象传输和接收数据包，但这会很慢且要求E1000内部缓存数据包数据。E1000使用直接内存访问（DMA）来直接从内存中读写数据包数据而不涉及CPU。这个驱动负责分配传输和接收队列的内存，设置DMA描述符，并用这些队列的位置设置E1000，但在此之后的一切都是异步的。为了传输一个数据包，驱动拷贝它到传输队列的下一个DMA描述符，然后通知E1000又有一个可获得的数据包；E1000会从描述符里拷贝出数据，在发送数据包的时候。同样的，当E1000接收一个数据包，它拷贝数据包到接收队列下一个DMA描述符，而驱动可以在下次机会时读取。  
  
接收和传输队列非常的相似，都由描述符序列组成。而这些具体描述符是不同的。每个描述符包含一些标志位和一个包含数据包数据的缓冲区的物理地址（可能是网卡要发送的数据包数据或者操作系统为网卡分配的缓冲区，用于将接收到的数据包写入）。  
  
队列以循环数组的方式实现，意味着当网卡或者驱动到达数组末尾，它返回到开头。二者都有一个头指针和一个尾指针，描述符的内容就在两个指针间。硬件总是从队首消耗描述符然后移动头指针，而驱动总是总是添加描述符到队尾然后移动尾指针。传输队列的描述符代表了等待被发出的数据包（因此，在稳定状态时，传输队列为空）。对于接收队列，描述符为网卡可以从中接收数据包的可用描述符（因此，在稳定状态时，接收队列全部由可用描述符组成）。正确地更新尾寄存器而不使E1000迷惑是很复杂的，要小心。  
  
这些数组的指针和描述符里数据包缓冲区地址一定都要是物理地址，因为硬件直接对RAM执行DMA而不用经过MMU。  
  
### 传输数据包
  
E1000的传输和接收功能基本上是相互独立的，所以可以一次完成一个。先从传输数据包开始因为在此前还不能测试接收。  
  
首先，需要初始化网卡来传输，跟着14.5节里描述的步骤（不要担心子小节）。传输初始化的第一步是设置传输队列。这个队列的精巧结构在3.4小节描述，描述符结构在3.3.3小节。本实验不使用E1000的TCP卸载功能，所以可以专注于“传统传输描述符格式”。现在应该阅读这些小节熟悉这些功能。  
  
#### C结构
  
用C结构描述E1000的结构是很方便的。就像见过的Trapframe结构一样，C结构可以精准地布局内存里的数据。C可以在字段间插入隔板，但E1000的结构布局不会有问题。如果确实遭遇了字段分配问题，检查GCC的packed属性。  
  
例如，思考下面的传统传输描述符：  
  

  63            48 47   40 39   32 31   24 23   16 15             0
  +---------------------------------------------------------------+
  |                         Buffer address                        |
  +---------------+-------+-------+-------+-------+---------------+
  |    Special    |  CSS  | Status|  Cmd  |  CSO  |    Length     |
  +---------------+-------+-------+-------+-------+---------------+  
  
结构的第一个字节从最右端开始，所以为了转换为C结构，从右读到左，从顶部读到底部。如果从右边看，会看到所有字段恰好放在一个标准大小类型里：  
```
struct tx_desc
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
};
```
  
驱动需要为传输描述符数组和传输描述符指向的数据包缓冲区保留内存。有几种方法完成这件事，从动态分配页到简单地在全局变量里声明它们。无论哪种，记住E1000直接访问物理内存，意味着所有它访问地缓冲区都是连续的物理内存。  
  
也有多种方法处理数据包缓冲区。最简单的，本实验最开始推荐，在驱动初始化时为每个描述符的数据包缓冲区保留空间。以太网数据包的最大大小为1518字节，设定了这些缓冲区应该多大。更多的先进驱动可以动态地分配数据包缓冲区（例如，在网络使用率较低时减少内存开销）或者直接传递用户空间提供的缓冲区（即零拷贝技术），但开始简单点也不错。  
  
> + Exercise5
	执行14.5小节描述地初始（不是子小节）。参考13小节完成初始化过程中寄存器的引用，参考3.3.3和3.4小节完成传输描述符和描述符数组。
	注意传输描述符数组的要求和数组长度的限制。因为TDLEN必须为128字节而每个传输描述符为16字节，传输描述符数组需要8的倍数个传输描述符。然而，不要使用多于64个描述符，不然测试就无法测试传输环溢出。
	对于TCTL.COLD，可以假设全双工操作。对于TIPG，参考13.4.34小节表13-77里为IEEE 802.3标准IPG的默认值。（不要用14.5小节表里的值。）
  
尝试运行`make E1000_DEBUG=TXERR,TX qemu`。如果使用了实验版本的QEMU，应该能看到一个 "e1000: tx disabled" ，在设置了TDT寄存器（因为这发生在设置TCTL.EN前)时而没有更多的e1000消息。  
  
传输初始化：为传输描述符表分配内存区域。软件确保这个区域分配在段落（16字节）边界上。编辑传输描述符基址（TDBAL/TDBAH）寄存器到这个区域。TDBAL供32位地址使用。  
  
传统描述符初始化：addr为缓冲区地址；length初始化为0；cso初始为0；cmd初始化0；status初始化为DD可用状态；css初始化为0；special初始化为零。  

设置传输描述符长度（TDLEN）寄存器到描述符环的大小。寄存器一定要分配128字节。  
  
传输描述符头和尾（TDH/TDT)寄存器在通电后初始化（被硬件）到0b或被软件初始化的以太网控制器复位。软件应该向这两个寄存器里写入0b。  
  
为要求的操作初始化传输控制寄存器（TCTL）包括以下步骤：  
> + 为普通操作设置使能位（TCL.EN)到1b。
> + 设置间隔短包（TCTL.PSP)位到1b。
> + 设置碰撞阈值（TCTL.CT)到需要的值。以太网标准为10h。这个设置只在半双工模式时有效。
> + 设置碰撞距离（TCTL.COLD)到它期待的值。对于全双工操作，这个值应为40h。
  
编辑传输IPG（TIPG）寄存器。表13-77里IEEE 802.3标准IPG默认值为：  
  
|字段|位|值|
|----|----|----|
|IPGT|9:0|10|
|IPGR1|19:10|8|
|IPGR2|29:20|6|
|Reserverd|31：30|0|
  
在e1000.h里添加定义和结构：  
```

/* Transmit Descriptor bit definitions */
#define TXD_CMD_EOP    0x01 /* End of Packet */
#define TXD_CMD_IFCS   0x02 /* Insert FCS (Ethernet CRC) */
#define TXD_CMD_IC     0x04 /* Insert Checksum */
#define TXD_CMD_RS     0x08 /* Report Status */
#define TXD_CMD_RPS    0x10 /* Report Packet Sent */
#define TXD_CMD_DEXT   0x20 /* Descriptor extension (0 = legacy) */
#define TXD_CMD_VLE    0x40 /* Add VLAN tag */
#define TXD_CMD_IDE    0x80 /* Enable Tidv register */
#define TXD_STAT_DD    0x01 /* Descriptor Done */
#define TXD_STAT_EC    0x02 /* Excess Collisions */
#define TXD_STAT_LC    0x04 /* Late Collisions */
#define TXD_STAT_TU    0x08 /* Transmit underrun */
#define TXD_CMD_TCP    0x01 /* TCP packet */
#define TXD_CMD_IP     0x02 /* IP packet */
#define TXD_CMD_TSE    0x04 /* TCP Seg enable */
#define TXD_STAT_TC    0x04 /* Tx Underrun */

#define TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define TDLEN    0x03808  /* TX Descriptor Length - RW */
#define TDH      0x03810  /* TX Descriptor Head - RW */
#define TDT      0x03818  /* TX Descripotr Tail - RW */
#define TCTL     0x00400  /* TX Control - RW */
#define TIPG     0x00410  /* TX Inter-packet gap -RW */

#define TCTL_EN     0x00000002    /* enable tx */
#define TCTL_PSP    0x00000008    /* pad short packets */
#define TCTL_CT     0x00000ff0    /* collision threshold */
#define TCTL_COLD   0x003ff000    /* collision distance */
 

struct tx_desc
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
}__attribute__((packed));
 
char txbuffer [NTXDESC][TXBUFFSIZE];

```
e1000_init里描述符初始化：  
```
struct tx_desc dxqueue[NTXDESC];

void txdesc_init(){
    memset(dxqueue,0,sizeof(struct tx_desc)*NTXDESC);
    memset(txbuffer,0,TXBUFFSIZE*NTXDESC);
    for (int i = 0 ; i < NTXDESC; ++i){
        dxqueue[i].addr = PADDR(txbuffer[i]);
        dxqueue[i].status = TXD_STAT_DD;//初始化为DD可用状态
    }
}
```  
tx寄存器初始化：  
```
void tx_init(){
    E1000REG(TDBAL) = PADDR(dxqueue);
    E1000REG(TDBAH) = 0;
    E1000REG(TDLEN) = NTXDESC * sizeof(struct tx_desc);
    E1000REG(TDH) = 0;
    E1000REG(TDT) = 0;
    E1000REG(TCTL) = TCTL_EN | TCTL_PSP |(TCTL_CT & (0x10 << 4))|(TCTL_COLD & (0x40 << 12));
    E1000REG(TIPG) = 10 | (8 << 10) | (12 << 20);

}
```
在e1000_init中调用：  
```
int e1000_init(struct pci_func *pcif){
    pci_func_enable(pcif);
    e1000 = mmio_map_region(pcif->reg_base[0],pcif->reg_size[0]);
    cprintf("e1000: status %x\n",E1000REG(DEVICE_STATUS));

    txdesc_init();
    tx_init();

    return  0;
}
````  

运行`make E1000_DEBUG=TXERR,TX qemu`得到输出：  
```
e1000: status 80080783
e1000: tx disabled
```
  
现在传输被初始化了，需要写一个传输数据包的代码，使它可以被用户空间通过系统调用访问。为了传输一个数据包，需要把它添加到传输队列的末尾，这意味着拷贝数据包数据到下一个数据包缓冲区并更新TDT（传输描述符尾）寄存器来通知网卡另一个数据包在传输队列里（注意TDT是传输描述符数组里的一个序列，而不是一个字节偏移量；文档关于此处没有明确0。   
  
然而传输队列只有这么大。如果网卡落后于传输数据包而传输队列满了呢？为了检测这种状况，需要一些E1000的反馈。不幸的是，不能只使用TDH（传输描述符头）寄存器，文档显式地说明了从软件读取这个寄存器是不可靠的。然而，如果设置了一个传输描述符指令字段的RS位，当网卡传输了这个描述符的数据包，这个网卡会设置描述符状态字段的DD位。如果一个描述符的DD位被设置了，就知道它是可以安全回收的，并用它来传输另一个数据包。  
  
当用户调用传输系统调用，但下一个描述符的DD没有被设置，提示传输队列为满呢？需要决定在这个状况下来做什么。可以简单地丢弃这个包。网络协议对于这样的事有恢复弹性，但如果丢弃了很多的数据包，协议也许就不能恢复了。作为替代可以通知用户环境它需要重试，就像在sys_ipc_try_send做的一样。这样做的好处是延后生成数据的环境。  
  
> + Exercise6
	写一个函数来传输一个数据包，通过检查下一个描述符是否可用，拷贝数据包数据到下一个描述符，然后更新IDT。确保处理传输队列满的情况。  
  
传输函数实现：  
```
//  驱动从尾指针写入数据包，网卡以头指针追赶发送数据包，空队列时二者重合
int txpacket(void *src,size_t length){
    uint32_t tail = E1000REG(TDT);
    struct tx_desc next = txqueue[tail];
    // 检查是否可用
    if (!(next->status & TXD_STAT_DD))
        return -1;
    if (length > TXBUFFSIZE)
        length = TXBUFFSIZE;
    
    memmove(txbuffer[tail],src,length);
    next->length = length;
    next->status &= ~TXD_STAT_DD;
    next->cmd = TXD_CMD_EOP | TXD_CMD_RS;//EOP指示为数据包尾，RS开启状态检查
    // 更新队尾
    E1000REG(TDT) = (tail + 1) % NTXDESC;
    return 0;
}
``` 
 
现在是测试数据包传输代码的好时机。尝试传输几个数据包，通过从内核直接调用传输函数。不需要创建符合某种网络协议的数据包来测试代码。运行`make E1000_DEBUG=TXERR,TX qemu`来测试，应该看到类似：  
`e1000: index 0: 0x271f00 : 9000002a 0`  
每行给出了传输数组里的一个序列，传输描述符的缓冲区地址，cmd/CSO/length字段，还有special/CSS/status字段。如果QEMU没打印出期待的值，检查是否填写了正确的描述符，是否正确设置TDBAL和TDBAH。如果得到了 "e1000: TDH wraparound @0, TDT x, TDLEN y" 这样的信息，说明E1000会一直在传输队列里循环而不会停下（如果QEMU不检查这项，他就会进入无限循环），可能是没有正确操作TDT。如果得到了很多"e1000: tx disabled"消息，说明没有正确设置传输控制寄存器。  
  
在i386_init调度环境前添加测试代码：  
```
	txpacket("test567",7);
	txpacket("test56789",9);
	txpacket("test56789abc",12);
	txpacket("test56789abcdef",15);
	txpacket("test567",7);
	txpacket("123456789123456789",18);
```
运行测试指令得到输出：  
```
e1000: tx disabled
e1000: index 0: 0x2afec0 : 9000007 0
e1000: index 1: 0x2b06c0 : 9000009 0
e1000: index 2: 0x2b0ec0 : 900000c 0
e1000: index 3: 0x2b16c0 : 900000f 0
e1000: index 4: 0x2b1ec0 : 9000007 0
e1000: index 5: 0x2b26c0 : 9000012 0

```  
  
一旦QEMU运行，可以运行`tcpdump -XXnr qemu.pcap`来检查传输的数据包数据。如果从QEMU看到了期待的 "e1000: index" 消息，但数据包抓取为空，再次检查是否填写了描述符里所有必要的字段和标志位（E1000也许传过了传输描述符而没有发现它需要发送什么东西）。  
  
运行tcpdump得到输出：  
```
reading from file qemu.pcap, link-type EN10MB (Ethernet)
22:58:50.488460 [|ether]
	0x0000:  7465 7374 3536 37                        test567
22:58:50.490860 [|ether]
	0x0000:  7465 7374 3536 3738 39                   test56789
22:58:50.492797 [|ether]
	0x0000:  7465 7374 3536 3738 3961 6263            test56789abc
22:58:50.493792 37:38:39:61:62:63 > 74:65:73:74:35:36, ethertype Unknown (0x6465), length 15: 
	0x0000:  7465 7374 3536 3738 3961 6263 6465 66    test56789abcdef
22:58:50.493866 [|ether]
	0x0000:  7465 7374 3536 37                        test567
22:58:50.493943 37:38:39:31:32:33 > 31:32:33:34:35:36, ethertype Unknown (0x3435), length 18: 
	0x0000:  3132 3334 3536 3738 3931 3233 3435 3637  1234567891234567
	0x0010:  3839                                     89

```
> + Exercise7
	添加一个系统调用从用户空间传输数据包。具体接口由自己决定。不要忘记检查所有从用户空间传递到内的指针。  
  
kern/syscall.c中定义：  
```
static int
sys_txpacket(void *src,size_t length)
{
    user_mem_assert(curenv, src, length, PTE_U);
	return txpacket(src,length);
}
```
添加分发case:  
```
		case (SYS_txpacket):
			return sys_txpacket((void *)a1,a2);
```
inc/syscall.h中注册系统调用号：  
```
enum {
	...
	SYS_txpacket,
	NSYSCALLS
};
```
inc/lib.h中添加声明：  
`int sys_txpacket(void *src,size_t length);`  
  
lib/syscall.c中添加定义：  
```
int
sys_txpacket(void *src, size_t length)
{
	return syscall(SYS_txpacket, 0, (uint32_t)src, length, 0, 0, 0);
}
```  
  
### 传输数据包：网络服务器
  
现在有设备驱动传输侧的系统调用接口了，是时候发送数据包了。输出助手环境的目标是在循环中完成下列工作：从核心网络服务器接收NSREQ_OUTPUT IPC消息并根据这些IPC消息发送发送数据包到网络设备服务器驱动，用之前添加的系统调用。NSREQ_OUTPUT类IPC被net/lwip/jos/jif/jif.c的low_level_output 函数发送，后者绑定了lwIP栈到JOS的网络系统。每个IPC都包括一个由Nsipc联合体组成的页，它其中jif_pkt的pkt结构字段（见inc/ns.h)包含了数据包。jif_pkt结构如下：  
```
struct jif_pkt {
    int jp_len;
    char jp_data[0];
};
```
  
jp_len代表了数据包的长度。IPC页上所有后续的字节都是数据包内容专用的。使用一个像jp_data的0长度数组作为结构体结束是一个常见的C语言技巧（有人认为是可憎的）来代表一个未定长的缓冲区。因为C语言不做数组边界检查，只要确保在下面的结构有足够的未使用内存，就可以把jp_data当作一个任意大小的数组。  
  
注意设备驱动，输出环境和核心网络服务器间的互动，尤其是在设备驱动传输队列没有更多空间的时候。核心网络服务器使用IPC服务器发送数据包到输出环境。如果输出环境由于一个发包系统调用终止，驱动没有更多的缓冲区空间留给新数据包，核心网络服务器会阻塞等待输出服务器接收IPC调用。  
  
> + Exercise8
	实现net/output.c。
  
output.c实现：  
```
void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// 	- read a packet from the network server
	while(1){
		if (ipc_recv(NULL,&nsipcbuf,NULL) != NSREQ_OUTPUT)
			continue;
		//	- send the packet to the device driver		
		while (sys_txpacket(nsipcbuf.pkt.jp_data,nsipcbuf.pkt.jp_len) < 0)
		// 传输失败时调度切换进程
		sys_yield();
	}
}

```
  
可用使用net/testoutput.c来测试输出代码而不用涉及整个网络服务器。尝试运行`make E1000_DEBUG=TXERR,TX run-net_testoutput`，应该看到如下的：  
```
Transmitting packet 0
e1000: index 0: 0x271f00 : 9000009 0
Transmitting packet 1
e1000: index 1: 0x2724ee : 9000009 0
...
```
而`tcpdump -XXnr qemu.pcap `应该输出：  
```
reading from file qemu.pcap, link-type EN10MB (Ethernet)
-5:00:00.600186 [|ether]
	0x0000:  5061 636b 6574 2030 30                   Packet.00
-5:00:00.610080 [|ether]
	0x0000:  5061 636b 6574 2030 31                   Packet.01
...
```
  
为了测试大量数据包，尝试`make E1000_DEBUG=TXERR,TX NET_CFLAGS=-DTESTOUTPUT_COUNT=100 run-net_testoutput`。如果整个指令溢出了传输环，再次检查是否正确处理了DD状态，通知硬件来设置DD状态位（使用RS指令位）。  
  
现在代码应该通过testoutput测试。  
  
运行testoutput.c测试，得到输出：  
```
Transmitting packet 0
Transmitting packet 1
e1000: index 6: 0x2b3ec0 : 9000009 0
block cache is good
superblock is good
bitmap is good
Transmitting packet 2
e1000: index 7: 0x2b46c0 : 9000009 0
Transmitting packet 3
e1000: index 8: 0x2b4ec0 : 9000009 0
Transmitting packet 4
e1000: index 9: 0x2b56c0 : 9000009 0
Transmitting packet 5
e1000: index 10: 0x2b5ec0 : 9000009 0
Transmitting packet 6
e1000: index 11: 0x2b66c0 : 9000009 0
Transmitting packet 7
e1000: index 12: 0x2b6ec0 : 9000009 0
Transmitting packet 8
e1000: index 13: 0x2b76c0 : 9000009 0
Transmitting packet 9
e1000: index 14: 0x2b7ec0 : 9000009 0
e1000: index 15: 0x2b86c0 : 9000009 0
```
tcpdump得到输出：
```
00:25:53.110034 [|ether]
	0x0000:  5061 636b 6574 2030 30                   Packet.00
00:25:53.118377 [|ether]
	0x0000:  5061 636b 6574 2030 31                   Packet.01
00:25:53.120937 [|ether]
	0x0000:  5061 636b 6574 2030 32                   Packet.02
00:25:53.122482 [|ether]
	0x0000:  5061 636b 6574 2030 33                   Packet.03
00:25:53.124186 [|ether]
	0x0000:  5061 636b 6574 2030 34                   Packet.04
00:25:53.125719 [|ether]
	0x0000:  5061 636b 6574 2030 35                   Packet.05
00:25:53.127425 [|ether]
	0x0000:  5061 636b 6574 2030 36                   Packet.06
00:25:53.129303 [|ether]
	0x0000:  5061 636b 6574 2030 37                   Packet.07
00:25:53.130702 [|ether]
	0x0000:  5061 636b 6574 2030 38                   Packet.08
00:25:53.131058 [|ether]
	0x0000:  5061 636b 6574 2030 39                   Packet.09
```
测试一百次数据包传输：  
```
...
Transmitting packet 98
e1000: index 39: 0x2c46c0 : 9000009 0
Transmitting packet 99
e1000: index 40: 0x2c4ec0 : 9000009 0
e1000: index 41: 0x2c56c0 : 9000009 0
```
testoutput测试通过，得分35/35。  
  
> + Qusetion
	1.如何架构传输实现？具体地说，当传输环满了后是怎么做的？
  
当传输队列满时，输出环境放弃当前传输并交还处理器控制权，等待下一次传输的机会。  
  
## Part B 接收数据包和网络服务器
  
### 接收数据包
  
就像为传输数据包所做的一样，需要设置E1000来接收数据包并提供一个接收描述符队列来接收描述符。3.2小节描述了数据包接收是如何工作的，包括接收队列结构和接收描述符，初始过程的细节在14.4小节。  
  
> + Exercise9
	阅读3.2小节。可以忽略关于中断与校验和卸载的部分（如果决定使用这些功能可以再来参考），不需要关心阈值的具体细节和网卡内部cache的工作。  
  
接收描述符格式：  
  63            48 47   40 39   32 31           16 15             0
  +---------------------------------------------------------------+
  |                         Buffer address                        |
  +---------------+-------+-------+-----------------+-------------+
  |    Special    | Errors| Status| Packet checksum |    Length   |
  +---------------+-------+-------+-----------------+-------------+  
  
转化为C语言结构：  
```
struct rx_desc
{
	uint64_t addr;
	uint16_t length;
	uint16_t pchecksum;
	uint8_t status;
	uint8_t errors;
	uint16_t special;
};
```  

接收队列很像传输队列，除了它由等待将要到来的数据包填充的数据包缓冲区组成。因此当网络空闲时，传输队列为空（因为所有的数据包都被发送了），但接收队列为满（都是空数据包缓冲区）。  
  
当E1000接收一个数据包，它首先检查它是否匹配网卡设置的过滤器（例如，检查数据包是否寻址到这个E1000的MAC地址）并忽略不匹配任何过滤器的数据包。否则，E1000尝试从接收队列里检索下一个接收描述符。如果头(RDH)已经达到了尾（RDT），则接收队列耗尽了空闲描述符，所以网卡丢弃了数据包。如果有空闲的接收描述符，它拷贝数据包数据到描述符指向的缓冲区，设置描述符的DD（描述符完成）位和EOP（数据包尾）状态位，并增加RDH。  
  
如果E1000接收了一个比一个接收描述符的数据包缓冲区更大的数据包，它会从接收队列检索必要多的描述符来存储整个数据包。为了指示这种情况的发生，网卡设置所有这些描述符的DD状态位，但只在最后的描述符设置EOP。可以在驱动里处理这种可能性，或者简单地设置网卡不要接收“长数据包”（也被称为巨型帧）并确保接收缓冲区足够大以存放最大可能的标准以太网帧（1518字节）。  
  
> + Exercise10
	设置接收队列并按照14.4小节的流程设置E1000。不需要支持“长数据包”或者多点传输。现在，不要设置网卡使用中断；如果决定使用接收中断可以在之后改变它。同样的，设置E1000来剥离以太网CRC，因为打分脚本期待它被剥离。  
	默认地，网卡会过滤所有的数据包。可以用网卡自己的MAC地址来设置接收地址寄存器（RAL和RAH）来接收寻址到网卡的数据包。可以简单地硬编码QEMU的默认MAC地址52:54:00:12:34:56（实验已经硬编码到lwIP里，所以在这里这么做不会让事情更糟）。千万小心字节序；MAC地址从低位字节写到高位字节，所以52:54:00:12时MAC地址的低32位，而34:56为高32位。
	E1000只支持一套具体的接收缓冲区大小（给定在RCTL.BSIZE里描述，见13.4.22小节）。如果使得接收数据包缓冲区足够大并关闭了长数据包，就不需要担心数据包横跨了数个接收缓冲区。同样的，记住就像传输，接收队列和数据包缓冲区必须在物理内存中连续。
	应该使用至少128个接收描述符。  
  
现在可以做一个接收功能的基础测试了，尽管没有写接收数据包的代码。运行`make E1000_DEBUG=TX,TXERR,RX,RXERR,RXFILTER run-net_testinput`。testinput会传输一个ARP（地址解析协议）公告数据包（使用数据包传输系统调用），QEMU会字段回应。尽管驱动还不能接收这个回应，可以看到一个"e1000: unicast match[0]: 52:54:00:12:34:56"小心，提示一个数据包被E1000接收了并匹配设置的接收过滤器。如果看到 "e1000: unicast mismatch: 52:54:00:12:34:56" 作为替代，E1000过滤掉了这个数据包，意味着没有正确设置RAL和RAH。确保正确处理了字节序，不要忘记设置RAH的地址有效位。如果没有获得任何的"e1000" 消息，说明没有正确地开启接收。  
  
接收初始化：用需要的以太网地址设置接收地址寄存器（RAL/RAH）。RAL[0]/RAH[0]应该被用来存储独立的以太网控制器的以太网MAC地址。  
初始化MTA（多点传输数组）到0。  
中断掩码设置（IMS）关闭。  
分配接收描述符列表的内存区域。确保分配的内存为段落对齐（16字节）。设置接收描述符基址（RDBAL/RDBAH）寄存器为区域的地址。  
设置接收描述符长度（RDLEN）寄存器为描述符环的大小（字节），这个寄存器一定要分配128字节。  
接收描述符头和尾寄存器被硬件初始化到0b，在通电后或者一个软件初始化的以太网控制器复位。应该分配正确大小的接收缓冲区，它们的地址的指针应该被存放在接收描述符环里。环境初始化接收描述符头（RDH）寄存器和接收描述符尾（RDT）为正确的头尾地址。头应该指向描述符换里第一个有效的接收描述符而尾应该指向描述符环里最后一个有效描述符后的描述符。  
设置接收控制（RCTL）寄存器为正确的值：  
> + 为普通操作设置接收器使能（RCTL.EN)位到1b。然而在接收描述符环初始化前最好保持该位逻辑关闭，直到接收描述符环初始化完成程序准备好处理接受包。  
> + 设置长数据包使能位（RCTL.LPE)关闭。
> + 回顾模式（RCTL.LBM）应该为普通操作被设置为00b。
> + 设置接收描述符最小阈值大小（RCTL.RDMTS)为0。
> + 设置广播接收模式（RCTL.BAM)位到1b来允许硬件接收广播数据包。
> + 设置接收缓冲区大小（RCTL.BSIZE)来反映软件提供给硬件的接收缓冲区大小。在接收缓冲区需要大于2048字节时设置缓冲区扩展位（RCTL.BSEX)。
> + 设置剥离以太网CRC位（RCTL.SECRC)。
  
e1000.h中定义接收描述符结构：  
```
struct rx_desc
{
	uint64_t addr;
	uint16_t length;
	uint16_t pchecksum;
	uint8_t status;
	uint8_t errors;
	uint16_t special;
} __attribute__((packed));
```
添加接收描述符相关定义：  
```
/* Receive Descriptor bit definitions */
#define RXD_STAT_DD       0x01    /* Descriptor Done */
#define RXD_STAT_EOP      0x02    /* End of Packet */
...
#define RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
#define RDLEN    0x02808  /* RX Descriptor Length - RW */
#define RDH      0x02810  /* RX Descriptor Head - RW */
#define RDT      0x02818  /* RX Descriptor Tail - RW */
#define RCTL     0x00100  /* RX Control - RW */
#define RA       0x05400  /* Receive Address - RW Array */
#define RAH_AV  0x80000000        /* Receive descriptor valid */

#define RCTL_EN             0x00000002    /* enable */
#define RCTL_LPE            0x00000020    /* long packet enable */
#define RCTL_BAM            0x00008000    /* broadcast enable */
#define RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
#define RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */

#define NRXDESC 128
#define RXBUFFSIZE 2048
```
声明接收缓冲区数组：  
`char rxbuffer [NRXDESC][RXBUFFSIZE];`  
e1000.c中初始化接收描述符数组：  
```
struct rx_desc rxqueue[NRXDESC];

void rxdesc_init(){
    memset(rxqueue,0,sizeof(struct rx_desc)*NRXDESC);
    memset(rxbuffer,0,RXBUFFSIZE*NRXDESC);
    for (int i = 0 ; i < NRXDESC; ++i)
        rxqueue[i].addr = PADDR(rxbuffer[i]);   
}
```
接收相关寄存器初始化：  
```
void rx_init(){
    // 网络字节序处理和有效位设置
    E1000REG(RA) = 0x52|(0x54<<8)|(0x00<<16)|(0x12<<24);
    E1000REG(RA+4) = 0x34|(0x56<<8)|RAH_AV;
    E1000REG(RDBAL) = PADDR(rxqueue);
    E1000REG(RDBAH) = 0;
    E1000REG(RDLEN) = NRXDESC * sizeof(struct rx_desc);
    E1000REG(RDH) = 0;
    E1000REG(RDT) = NRXDESC - 1;
    E1000REG(RCTL) = RCTL_EN | RCTL_BAM |RCTL_SECRC;

}
```
在e1000_init中初始化：  
```
int e1000_init(struct pci_func *pcif){
    pci_func_enable(pcif);
    e1000 = mmio_map_region(pcif->reg_base[0],pcif->reg_size[0]);
    cprintf("e1000: status %x\n",E1000REG(DEVICE_STATUS));

    txdesc_init();
    tx_init();

    rxdesc_init();
    rx_init();
    
    return  0;
}
```
运行测试得到输出：
```
Sending ARP announcement...
e1000: index 0: 0x2b0ec0 : 900002a 0
e1000: unicast match[0]: 52:54:00:12:34:56
```
  
现在可以实现接收数据包。为了接收一个数据包，驱动需要保持追踪哪个描述符来存放下一个接收的数据包（提示：由设计决定，E1000里可能已经有一个寄存器在追踪此事）。就像传输，文档声明RDH寄存器从程序读取是不可靠的，所以为了确定一个数据是否被分发到这个描述符的数据包缓冲区，需要读取描述符的DD状态位。如果DD位被设置了，可以从描述符的数据包缓冲区拷贝出数据包，然后通知网卡描述符可用，通过更新队列尾序列RDT。  
  
如果DD没有被设置，那么没有数据包被接收。这等同于是接收侧的传输队列满，这种情况下有几件事可以做。可以简单地返回一个“try again”错误并要求调用者重试。这个方法对于满传输队列很有效，因为那是个传输情景，但对空队列就不太合理，因为接收队列可能保留空很长一段时间。第二种方法是暂停调用环境直到接收队列里有数据包来处理。这个策略很像sys_ipc_recv。就像在IPC里 ，因为每个CPU只有一个内核栈，在离开内核时栈上的状态会丢失。需要设置一个标志位提示环境被接收队列延留了并记录系统调用参数。这个方法的弱点是复杂性：E1000必须被指示来生成接收中断，驱动必须处理它们来从阻塞等待一个数据包里恢复环境。  
  
> + Exercise 11
	写一个函数来从E1000接收一个数据包并提供给用户空间使用，以系统调用的方式。确认处理了接收队列空的状况。
  
函数实现：  
```
// 网卡向头指针写入数据包，驱动用尾指针追赶读取数据包，满队列时尾在队列中头之后一位
int rxpacket(void* dst){
    uint32_t tail = (E1000REG(RDT) + 1 ) % NRXDESC;
    struct rx_desc *next = &rxqueue[tail]; 
    // 不可用表示无数据需要读取
    if (!(next->status & RXD_STAT_DD))
        return 0;
        
    memmove(dst,rxbuffer[tail],next->length);
    // 清空可用位
    next->status &= ~RXD_STAT_DD;
    E1000REG(RDT) = tail;
    // 返回读取字节数
    return next->length;
}
```
系统调用添加操作如前。
```
static int
sys_rxpacket(void *dst)
{
    // 检查用户环境访问权限
	user_mem_assert(curenv, dst, RXBUFFSIZE, PTE_U);
	return rxpacket(dst);
}
```
  
### 接收数据包：网络服务器
  
在网络服务器输入环境里，需要使用新接收系统调用来接收数据包并传递它们到核心网络服务器，使用NSREQ_INPUT类IPC消息。这些IPC输入消息应该附有带着Nsipc联合体的页，它其中的jif_pkt结构体pkt字段位从网络接收的数据包。
  
> + Exercise12
	实现net/input.c。
  
再次运行testinput，使用`make E1000_DEBUG=TX,TXERR,RX,RXERR,RXFILTER run-net_testinput`。应该看到：  
```
Sending ARP announcement...
Waiting for packets...
e1000: index 0: 0x26dea0 : 900002a 0
e1000: unicast match[0]: 52:54:00:12:34:56
input: 0000   5254 0012 3456 5255  0a00 0202 0806 0001
input: 0010   0800 0604 0002 5255  0a00 0202 0a00 0202
input: 0020   5254 0012 3456 0a00  020f 0000 0000 0000
input: 0030   0000 0000 0000 0000  0000 0000 0000 0000
```
由input开头的行为QEMU的ARP回应16进制抓取。  
  
代码现在应该通过testinput测试，注意没有方法不通过发送至少一个ARP数据包来通知QEMU JOS的IP地址以测试数据包接收，所以传输代码里的bug会引起这个测试失败。  
  
为了更全面地测试网络代码，提供了一个叫echosrv的守护进程，设置一个回声服务器运行在端口7，返回所有通过TCP连接发送过来的东西。使用`make E1000_DEBUG=TX,TXERR,RX,RXERR,RXFILTER run-echosrv `来在一个终端开始回声服务器，然后在另一个终端使用`make nc-7`连接到它。打出的每行都应该被服务器传回。每次模拟E1000接收一个数据包，QEMU应该打印类似下面的东西到控制台：  
```
e1000: unicast match[0]: 52:54:00:12:34:56
e1000: index 2: 0x26ea7c : 9000036 0
e1000: index 3: 0x26f06a : 9000039 0
e1000: unicast match[0]: 52:54:00:12:34:56
```
此时，应该能通过echosrv测试。  
  
input.c实现：  
```
void
input(envid_t ns_envid)
{
	binaryname = "ns_input";
	char buf[2048];
	size_t len;

	// LAB 6: Your code here:
	while (1){

		// 	- read a packet from the device driver
		// 接收队列为空时交还CPU控制权
		if ((len = sys_rxpacket(buf)) == 0){
			sys_yield();
		}
		else{
		//	- send it to the network server
		memcpy(nsipcbuf.pkt.jp_data, buf, len);
		nsipcbuf.pkt.jp_len = len;
		ipc_send(ns_envid,NSREQ_INPUT,&nsipcbuf,PTE_U|PTE_P);
		cprintf("input once\n");	
		// Hint: When you IPC a page to the network server, it will be
		// reading from it for a while, so don't immediately receive
		// another packet in to the same physical page.
		for(int i=0; i<500; i++)
			sys_yield();
		}
	}
}
   
```
运行`make E1000_DEBUG=TX,TXERR,RX,RXERR,RXFILTER run-net_testinput`得到输出：  
```
e1000: index 0: 0x2b2ec0 : 900002a 0
e1000: unicast match[0]: 52:54:00:12:34:56
Device 1 presence: 1
input: 0000   5254 0012 3456 5255  0a00 0202 0806 0001
input: 0010   0800 0604 0002 5255  0a00 0202 0a00 0202
input: 0020   5254 0012 3456 0a00  020f 0000 0000 0000
input: 0030   0000 0000 0000 0000  0000 0000 0000 0000
```
make grade通过testinput,echosrv测试。
  
### 网页服务器
  
一个最简单的网页服务器实现是发送文件到请求的客户端。实验已经提供了一个很简单的网页服务器框架代码，在user/httpd.c里。框架代码处理将要到来的连接并分析报文头。  
  
> + Exercise13
	网页服务器确实了处理发回文件内容给客户端的代码。实现send_file和send_data来完成网页服务器。  
  
一旦完成了网页服务器，用`make run-httpd-nox`来启动它，用喜欢的浏览器访问http://host:port/index.html，host为运行QEMU的计算机ip（当浏览器和QEMU在同一台计算机上时为localhost)，port为指令`make which-ports`报告的端口号。应该看到JOS内部的HTTP服务器提供的一个网页。  
  
现在，make grade得分应该为105/105。  
  
分析客户端处理函数：  
```
static void
handle_client(int sock)
{
	struct http_request con_d;
	int r;
	char buffer[BUFFSIZE];
	int received = -1;
	struct http_request *req = &con_d;

	while (1)
	{
		// Receive message
		// 读取客户端消息
		if ((received = read(sock, buffer, BUFFSIZE)) < 0)
			panic("failed to read");

		memset(req, 0, sizeof(*req));

		req->sock = sock;
		// 分析http要求
		r = http_request_parse(req, buffer);
		if (r == -E_BAD_REQ)
			send_error(req, 400);
		else if (r < 0)
			panic("parse failed");
		else
		// 发送文件
			send_file(req);

		req_free(req);

		// no keep alive
		break;
	}

	close(sock);
}
```
其中http_request_parse（）将请求的url放入req中传递给send_file。  
  
send_file打开请求的url来读取，如果文件不存在或是一个目录，用send_err发送一个404错误，设置file_size为文件的大小。  
  
```
static int
send_file(struct http_request *req)
{
	int r;
	off_t file_size = -1;
	int fd;
	struct Stat* st;
	// url文件无法打开
	if (fd = open(req->url,O_RDONLY) < 0)
		return send_error(req,404);
	// 获取文件描述符状态
	if (r = fstat(fd,st) < 0)
		return send_error(req,404);
	// 判断是否为目录
	if (st->st_isdir)
		return send_error(req,404);

	file_size = st->st_size;

	if ((r = send_header(req, 200)) < 0)
		goto end;

	if ((r = send_size(req, file_size)) < 0)
		goto end;

	if ((r = send_content_type(req)) < 0)
		goto end;

	if ((r = send_header_fin(req)) < 0)
		goto end;

	r = send_data(req, fd);

end:
	close(fd);
	return r;
}

static int
send_data(struct http_request *req, int fd)
{
	// LAB 6: Your code here.
	char buff[BUFFSIZE];
	int r,n;
	while(r = read(fd, buff, BUFFSIZE) > 0){
		if (write(req->sock, buff, BUFFSIZE) != r)
			return -1;
		n += r;
	}
	return n;
	//panic("send_data not implemented");
}
```
运行make which-ports获得端口号：  
`Local port 25002 forwards to JOS port 80 (web server)`  
  
远程访问主机服务器：http://xxx.xxx.xxx.xxx:25002/index.html  
  
make grade得分105/105。

至此，本实验完成。