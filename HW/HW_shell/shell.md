#Homework:shell
这次的任务是通过实现小型shell的几个功能熟悉Unix系统调用交互和shell。可以在任一支持Unix API的操作系统来完成这个任务。  
  
阅读[XV6_book]（https://pdos.csail.mit.edu/6.828/2018/xv6/book-rev10.pdf）的第零章。    
  
下载[6.828shell]（https://pdos.csail.mit.edu/6.828/2018/homework/sh.c）并通读。6.828shell包含两个主要部分：分析shell命令和实现它们。分析器只能识别如下的简单shell命令：  
```
ls > y
cat < y | sort | uniq | wc > y1
cat y1
rm y1
ls |  sort | uniq | wc
rm y
```
剪切粘贴这些命令到t.sh文件里。  
  
为了编译sh.c，选哟一个c编译器，例如gcc。通过指令`gcc sh.c`来编译这个空空如也的shell。这会产生一个a.out文件，通过`./aout < t.sh来运行。  
  
在还没有实现功能时，现在执行会打印错误信息。  
  
##执行简单命令  
实现简单命令，例如：  
  
`ls`  
  
分析器已经搭建了一个execcmd，只需要完成runcmd中的' 'case。查阅exec操作手册会很有帮助；输入“man 3 exec”，阅读execv的部分。在执行失败的时候打印一个错误信息。  
  
编译并执行a.out来测试程序。  
  
`./a.out`  
  
之后会打印一个提示符并等待输入。sh.c打印6.828$作为提示符避免与计算机的shell混淆。  
  
向shell输入  
  
`6.828$ ls`  
  
shell可能会报错（除非工作目录里有一个名为ls的程序或者使用了搜索PATH的exec版本）。重新输入  
  
`6.828$ /bin/ls`  
  
这会执行/bin/ls程序，打印工作目录下的所有文件名。可以通过输入ctrl-d来停止6.828shell，回到计算机的shell。 
  
如果有野心的话，可以实现对PATH变量的支持，使得在别的工作目录里也能直接调用程序而不用输入绝对路径。 

```
  case ' ':
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      _exit(0);
    // Your code here ...
    //access判断文件访问权限 在当前目录则执行命令
    if (access(ecmd->argv[0], F_OK) == 0) 
    {
		execv(ecmd->argv[0], ecmd->argv);
	  }
    //当前目录无执行对象则在/bin/中寻找
	  else 
    {
      char path[20] = "/bin/";                
      strcat(path, ecmd->argv[0]);         
        if(access(path, F_OK) == 0)         
            execv(path, ecmd->argv);
        else {                          
            fprintf(stderr, "%s: Command not found.\n", ecmd->argv[0]);       
        }
	  }
    break;
``` 
  
## I/O重定向 
  
实现I/O重定向来执行：  
  
```
echo "6.828 is cool" > x.txt
cat < x.txt
```
  
分析器已经能够识别“>”和“<”，并构建了一个redircmd,只需要完runcmd中这些符号的缺失代码。操作手册中关于打开和关闭的部分会很有用。  
  
注意redircmd中的mode域包含访问模式（如O_RDONLY），应该传入flags参数给open；在parsedirs中查找shell使用的模式值，和操作手册中open里flags参数的部分。  
  
确保当系统调用失败时打印错误信息。  
  
确认实现能够用上面的测试输入来正确运行。一个常见的错误是忘了说明文件创建必要的许可（如open的第三个参数）。  

```
  case '>':
  case '<':
    rcmd = (struct redircmd*)cmd;
    // Your code here ...
    //关闭默认文件描述符
    close(rcmd->fd);
    //打开目标文件
    if (open(rcmd->file, rcmd->flags, S_IWUSR|S_IRUSR) < 0) {
        fprintf(stderr, "file:%s can not open\n",rcmd->file);
        exit(0);
    }
    runcmd(rcmd->cmd);
    break;
```
  
## 实现管道  
  
实现管道来执行：    
  
`$ ls | sort | uniq | wc`  
  
分析器已经能够识别“|”，并构建了pipecmd，只需要完成runcmd中的“|”case。操作手册中pipe,fork,close和dup会很有用。  
  
用上面的管道行来测试运行。sort程序也许在/usr/bin里，在这种情况下可以通过输入绝对路径来运行sort。（可以通过`which sort`来找到shell的搜索路径里哪个目录下有名为sort的可以执行文件。）  

```
  case '|':
    pcmd = (struct pipecmd*)cmd;
    // Your code here ...
    if(pipe(p)<0)
    {
      printf(stderr,"can not creat pipe\n");
      exit(0);
    }
    if(fork1()==0)
    {
      close(1);//关闭标准输出
      dup(p[1]);//重定向到管道写端
      close(p[0]);//关闭管道读写描述符
      close(p[1]);
      runcmd(pcmd->left);
    }
    wait(&r);//等待子进程返回
    if(fork1()==0)
    {
      close(0);//关闭标志输入
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[1]);
    close(p[0]);
    wait(&r);
    break;
```
  
至此应该可以正确的运行下列指令：  
  
`6.828$ a.out < t.sh`  
  
确认程序使用了正确的绝对路径名。  
  
