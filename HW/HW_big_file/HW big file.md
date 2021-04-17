#Homework bigger files 
  
本节安排为增加一个xv6文件的最大尺寸。现在xv6文件被限制为140个扇区或者71680字节。这个限制来来自于一个xv6节点有12个“直接”区块数和一个“单一间接”区块数，指多达128个的额外区块数，总计128+12=140个。改变xv6文件系统代码来让每个阶段支持一个“双重间接”区块，包含128个单一间接区块，每个可以容纳多达128个数据区块的地址。结果使得一个文件可以由多达16523个扇区组成（或者大约8.5MB)。  
  
##预先准备
  
修改Makefile的CPUS定义为：  
  
`CPUS := 1`  
  
添加  
  
`	`  
  
到QEMUOPTS之前。  
  
上面两步显著加速了qemu创建大文件的速度。  
  
mkfs初始化文件系统，有着比1000更少的可用数据区块，太少以至于不能展示本节的成果。修改param.h来设置FSSIZE：  

`    #define FSSIZE       20000  // size of file system in blocks`  
  
下载[big.c](https://pdos.csail.mit.edu/6.828/2018/homework/big.c）到xv6目录，添加它到UPROGS列表中，然后运行big。它创建了xv6允许的最大文件，然后报告尺寸。现在应该时140个扇区。  
  
##要看什么
  
硬盘节点的格式定义在fs.h的dinode结构里。尤其注意其中的NDIRECT, NINDIRECT, MAXFILE,和the addrs[]元素。结构如图：  
  
查找硬盘上数据的代码在fs.c的bmap()里。通读它确保自己理解它做了什么。读写文件时都会调用bmap()。当写入时，bmap()分配新区块来存放文件内容，在有需要时分配一个间接区块来保存区块地址。  
  
bmap()处理两种区块数。bn参数时逻辑区块--一个关于文件起始位置的区块数。ip->addrs[]里的区块数的和 bread()的参数是硬盘区块数。可以把bamp()看作是映射文件逻辑区块数映射到硬盘区块数。  
  
##任务
  
修改bmap()使得它实现一个双重间接区块，作为直接区块和单一间接区块的补充。只有11个直接区块而不是12个来为新双重间接区块创造空间；不能改变硬盘节点的大小。ip->addrs[]的前11个元素应该是直接区块；第十二个应该为一个单一间接区块（和现在的一样）；第十三个为新双重间接区块。  
  
不必修改xv6来处理双重间接区块的删除。  
  
如果顺利的话，big会报告它能写入1653个扇区。这会使big划分数十秒来完成。  
  
##提示
  
确保理解了bmap()。写出ip->addrs[]的关系图，间接区块，双重间接区块，单一间接区块，和数据区块。理解为什么添加一个双重间接区块使得文件最大尺寸增加了16384个区块（实际为16383，因为需要减去一个直接区块数1）。  
  
思考如何序列化双重间接区块，和它指向的间接区块，使用逻辑区块数。  
  
如果改变了NDIRECT的定义，也要改变file.h里inode的addrs[]大小。确保inode结构和dinode结构在它们的addrs[]里有着同样的元素数。  
  
如果改变了NDIRECT的定义，确保超键了一个新的fs.img，因为mkfs也使用NDIRECT来构建初始化文件系统。如果删除了fs.img，在Unix（不是xv6)上make会构建一个新的。  
  
如果文件系统进入了坏状态，也许因为崩溃，删除fs.img(从Unix里）。make构建一个新的干净文件系统映像。  
  
不要忘记brelse()每个bread()的区块。  
  
只在需要时分配间接区块和双重间接区块，就像原版的bmap()一样。  
  
新关系图为  
  

修改fs.h,NDIRECT减一:  
  
`#define NDIRECT 11`  
  
添加定义NDBINDIRECT：  

`#define NDBINDIRECT NINDIRECT * NINDIRECT`  
  
修改最大文件大小：  
  
`#define MAXFILE (NDIRECT + NINDIRECT + NDBINDIRECT)`  
  
修改dinode结构和inode结构的addrs数组：  

`  uint addrs[NDIRECT+2];   // Data block addresses`  
  
修改bmap.c的实现  
  
```
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a, *b;
  struct buf *bp, *bnp;

  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0)
      ip->addrs[bn] = addr = balloc(ip->dev);
    return addr;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
    // Load indirect block, allocating if necessary.
    if((addr = ip->addrs[NDIRECT]) == 0)
      ip->addrs[NDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }
  bn -= NINDIRECT;

  if(bn < NDBINDIRECT){
    // 加载单一间接区块，必要时分配
    if((addr = ip->addrs[NDIRECT+1]) == 0) 
      ip->addrs[NDIRECT+1] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn/NINDIRECT]) == 0){
      a[bn/NINDIRECT] = addr = balloc(ip->dev);
      log_write(bp);
    }
    // 加载双重间接区块，必要时分配
    bnp = bread(ip->dev, addr);
    b = (uint*)bnp->data;
    if ((addr = b[bn%NINDIRECT]) == 0){
      b[bn%NINDIRECT] = addr = balloc(ip->dev);
      log_write(bnp);
    }
    brelse(bnp);
    brelse(bp);
    return addr;
  }
  panic("bmap: out of range");
}
```  
  
开启模拟器运行big，得到输出：  
```
$ big
.....................................................................................................................................................................
wrote 16523 sectors
done; ok
```
  
至此，本节完成。