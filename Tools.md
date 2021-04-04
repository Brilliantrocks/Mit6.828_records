# 课程工具及环境配置
选择云服务器作为实机
## 编程链检查
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
