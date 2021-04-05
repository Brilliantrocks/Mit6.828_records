# Lab1  
## Part a. PC Bootstrap 电脑启动引导程序  
这一部分实验不需要写代码，	而要求熟悉x86汇编。  
课程推荐了一系列整个6.828会涉及到的[读物](https://pdos.csail.mit.edu/6.828/2018/reference.html)作为参考，在Lab1里还为不熟悉汇编语言的人推荐了[PC Assembly Language Book](https://pdos.csail.mit.edu/6.828/2018/readings/pcasm-book.pdf)。值得一提的是，这本指导书中的例子是以Intel语法风格为NASM汇编器写的，而我们用到的GNU汇编器用到的代码是以AT&T语法风格写的。两种语法风格的简单转换参见[Brennan's Guide to Inline Assembly](http://www.delorie.com/djgpp/doc/brennan/brennan_att_inline_djgpp.html)。  
### Exercise 1 
熟悉参考读物里关于汇编语言的部分，推荐搭配阅读两种语法风格转换的文章。  
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
