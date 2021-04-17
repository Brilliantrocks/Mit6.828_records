#HW Threads and Locking
本节需要通过一个哈希表用线程和锁探索并行编程。需要在一个有多核的真实计算机上进行。  
  
下载[ph.c](https://pdos.csail.mit.edu/6.828/2018/homework/ph.c)并在自己的计算机上编译而不需要6.828提供的工具。   
  
```
$ gcc -g -O2 ph.c -pthread
$ ./a.out 2
```
  
2指明了对哈希表执行put和get操作的线程数目。  
  
在运行一段时间后，程序会产生类似下面的输出：  
```
1: put time = 0.003338
0: put time = 0.003389
0: get time = 7.684335
0: 17480 keys missing
1: get time = 7.684335
1: 17480 keys missing
completion time = 7.687856
```
  
每个线程运行在两个阶段。第一部分每个线程放入NKEYS/nthread个键值到哈希表里。第二步每个线程从哈希表里获取NKEYS个数据。打印的语句展示了每个线程每步花费了多少时间。底部的完成时间是应用总的完成时间。在上面的输出中，应用完成时间大约是7.6秒。每个线程计算了大约7.6秒。  
  
为了检查双线程是否改进了运行，试着用单一线程处理的结果比较：  
```
$ ./a.out 1
0: put time = 0.004073
0: get time = 6.929189
0: 0 keys missing
completion time = 6.933433
```  
单一线程的完成时间略低于双线程条件，但双线程在get部分时完成了单线程两倍的工作。因此双线程条件用两个处理核心在get阶段获得了近两倍的并行加速，这是很棒的。  
  
当运行这个应用时，如果在单核机器上运行或者机器忙于运行其他应用的话就看不到并行机制了。  
  
两个关键点：1.双线程有着几乎一样的完成时间，但双线程执行了两倍的get操作，如此就获得了很好的并行效果。2.双线程输出显示有很多键值找不到了。在实际运行中可能会有或多或少的偏差。如果在单线程下执行的话，就没有键值消失。  
  
为什么在多线程下会出现键值消失而单线程不会呢？找到使得双线程键值丢失的事件语句。 
  
put调用的insert改变了哈希表的记录，当并行执行时会覆盖另一个线程的操作，导致修改链表指针时丢失目标。 
  
为了避免这个事件的发生，在get和put前插入上锁和解锁语句，使得没有键值消失：  
  
```
pthread_mutex_t lock;     // declare a lock
pthread_mutex_init(&lock, NULL);   // initialize the lock
pthread_mutex_lock(&lock);  // acquire lock
pthread_mutex_unlock(&lock);  // release lock
```
  
在单线程和双线程条件下测试代码。   
  
加锁后双线程能够正确插入读取哈希表，而同一时间只有一个线程对哈希表操作，丧失了并行效果，完成时间大幅增加。  
  
修改代码使得get操作在获得正确输出的前提下运行在并行模式。（提示：应用中的get锁是必要的吗？）  
  
get操作不改变哈希表的数据，故是并行安全的操作，可以去锁。  
  
修改代码使得put操作在获得正确输出的前提下运行在并行模式。（提示：每个bucket都需要锁吗？）  
  
当put操作的bucket不同时并行操作并不会互相干扰，则对每个bucket分别加锁。
声明    
`pthread_mutex_t lock[NBUCKET];`  
初始化  
```
  for ( i = 0; i < NBUCKET; i++)
    pthread_mutex_init(&lock[i], NULL);   // initialize the lock  
```
put加锁修改  
```
static
void put(int key, int value)
{
int i = key % NBUCKET;
pthread_mutex_lock(&lock[i]);
insert(key, value, &table[i], table[i]);
pthread_mutex_unlock(&lock[i]);
}
```
修改后双线程下put阶段的运行时间缩短了。
  
至此本节完成