# 课程工具及环境配置
选择云服务器作为实机
## 1.编译链检查

输入
` % objdump -i `  
第二项应该为elf32-i386  
输入
`% gcc -m32 -print-libgcc-file-name`  
这条指令应该输出类似/usr/lib/gcc/i486-linux-gnu/version/libgcc.a 或 /usr/lib/gcc/x86_64-linux-gnu/version/32/libgcc.a的的东西。  
实际输出为/usr/lib/gcc/x86_64-linux-gnu/7/32/libgcc.a  
以上正常则编译链可用  
否则，输入以下指令以修复  
`% sudo apt-get install -y build-essential gdb`  
64位机器上需要安装32位gcc拓展库，以避免"__udivdi3 not found" 和 "__muldi3 not found"错误出现  
指令  
`% sudo apt-get install gcc-multilib`  
## 2.QEMU模拟器安装  

课程使用版本 QEMU version 2.3.0  
git项目源：https://github.com/mit-pdos/6.828-qemu.git  
克隆指令  
`git clone https://github.com/mit-pdos/6.828-qemu.git qemu`  
若出现EOF提前终止类提示 则输入一下指令提高git缓存  
`% sudo git config --global http.postBuffer 2000000000`   
`% sudo git config --global https.postBuffer 2000000000`  
安装需要几个额外依赖库  
```
% sudo apt install libsdl1.2-dev
% sudo apt install libtool-bin
% sudo apt install libglib2.0-dev
% sudo apt install libz-dev
% sudo apt install libpixman-1-dev
```  
configure设置  
`%./configure --disable-kvm --disable-werror --target-list="i386-softmmu x86_64-softmmu"`
有出现需要python2的状况，输入指令安装  
`% sudo apt install python`  
cd转到qemu目录，执行qemu安装  
`% make&&make install`  
## JOS操作系统  

克隆指令  
`git clone https://pdos.csail.mit.edu/6.828/2018/jos.git lab`  
cd转到lab目录  
执行make
`make`  
得到obj/kern/kernel.img则成功，已经准备好运行qemu  
执行`make qemu`或者`make qemu-nox`，可能需要root权限    
前者输出图形窗口，需要安装图形界面。普通用户远程ssh登陆操作时注意加上sudo来获得正常窗口输出    
后者则直接输出在终端，显示如下  
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
