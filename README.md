# MIT6.828 xv6 operating system engineering
记录学习笔记  
## 课程目标
OS操作系统开发学习  
## 环境配置
腾讯云服务器 S5.SMAL2规格  
操作系统 Ubuntu Server 18304.1 LTS 64bit 1核 cpu 2GB 内存  
Xshell7 SSH远程操作
Xmanager7 图形界面支持  

## Lab1 
  
实验的第一步从硬件启动开始，首先是对QEMU编译环境等工具的配置，然后推荐了课题相关的参考材料，lab1里涉及了很多汇编语言的内容，需要区分两种汇编语法的差异。  
lab1主要涵盖了计算机设备通电后引导启动至进入内核，从实模式进入保护模式的部分，其中通过gdb调试深入了引导启动的机器指令流程，并实现了部分控制台输出格式和代码反向追踪函数。  

### Part a.熟悉x86汇编语言，QEMU模拟器，pc开机引导程序  
### Part b.检验内核的boot loader引导加载程序  
### Part c.深入JOS内核初始化模板  
  
## Lab2 内存管理
  
紧接着lab1的内容后，lab2着眼于内存管理的相关内容。实现分页机制，首先是对物理内存分页管理，实现物理内存二级分页记录，页分配和页回收。之后实现一套页表映射管理的相关操作函数，依据JOS虚拟地址布局依次映射相应的内核虚拟地址空间及其权限。

### part a.物理页管理
### part b.虚拟内存
### part c.内核地址空间

## Lab3 用户环境
  
本课题的术语环境等效于常见的进程，lab3实现了JOS内用户进程，并处理用户进程的系统调用和异常处理。首先是定义环境（进程）结构体记录环境的相关信息加以管理。实现了环境的初始化，内存映射，创建，分配内存，加载二进制ELF文件，运行功能。然后是中断和异常处理机制，实现了中断描述符表（IDT）的初始化，汇编语言的陷入注册和处理。完成陷入处理机制的基础后实现了页错误，断点异常和系统调用的分发处理。在系统调用中通过行内汇编实现了陷入帧的保存和恢复。在页错误处理等函数中检查用户内存操作权限，以达到内存保护的目的。
  
### Part a. 用户环境和异常处理
### Part b. 页错误，断点意外和系统调用
 
## Lab4 抢占式多任务
  
lab4基于lab3的工作实现了多CPU下多进程的并发/并行。首先是LAPIC多处理器的相关支持，包括应用处理器的内存映射和初始化和陷入初始化，通过大内核锁处理环境间的竞态。以Round-Robin算法实现多环境调度，并实现调度系统调用以在用户环境中调用。之后处理多环境下的用户空间页错误以支持写时拷贝fork，通过环境页错误处理函数设置系统调用来让用户环境处理页错误，用汇编语言实现了用户环境异常帧的保存和恢复。然后实际完成写时拷贝fork的环境创建，页映射复制和写时拷贝页错误处理。最后开启外部中断使得内核可以从用户环境中抢回CPU控制权，汇编语言注册外部中断的陷入，并实现时钟中断的处理，调用调度函数重获CPU。多进程机制完成后实现进程间通信IPC，包括IPC接收和发送的系统调用。
  
### Part a. 多处理器支持和协同任务
### Part b. 写时拷贝Fork
### Part c. 抢占式多任务和进程间通信
  
## Lab 5 文件系统,Spawn and Shell
  
lab5实现了一个简单的文件系统。实际具体需要实现读取到区块cache，分配磁盘分区，映射文件偏移量到磁盘区块，IPC接口中的读写功能。spawn函数创建一个新环境，并通过文件系统加载一个程序镜像到子环境中运行，它利用环境陷入帧设置系统调用来初始化新环境状态。之后添加页表记录的一个共享属性，修改fork并在spawn中实现页共享。开启键盘中断以支持shell，处理键盘中断和串口中断陷入。然后实现shell中的重定向功能。  
  
## Lab 6 网络驱动
  
lab6为课题默认的最终实验，参考Intel的PCI系列以太网控制器软件开发者手册从0开始完成e1000网卡的驱动，并能够支持网路服务器。首先在时钟中断处理中添加时间戳计数，然后实现系统时间的系统调用。使用PCI设备接口注册e1000的设备信息和附属函数。在e1000的初始化中为其分配IO映射内存，然后参考手册完成传输和接收相关的队列和操作函数实现。网络服务器采用了lwIP架构，具体需要用e1000的传输接收系统调用实现其输出输入环境。最后通过实验版本的QEMU端口实现访问网页服务器。
  
### Part a.驱动初始化和传输数据包
### Part b.接收数据包和网络服务器
  
支此，全部实验作业完成，感谢MIT提供的优秀Lab和各位前辈的帮助。
