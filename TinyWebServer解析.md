# TinyServer

## 总览

>- config： 配置类
>- log: 日志类
>   - block_queue: 阻塞队列类




***
## 详解

### block_queue 
>阻塞队列: 
>- 使用单例模式创建日志系统， 对服务器运行状态、错误信息和访问数据进行记录。
>- 使用异步方式进行写入。
>- 使用`生产者-消费者`模型进行封装，使用循环数组实现队列，作为两者共享的缓冲区。
***
#### 单例模式
>- **单例模式**
>   - 保证一个类只创建一个实例，同时提供全局访问的方法。该实例被所有程序模块共享。
>       - 实现思路: 
>           - 私有化构造函数，以防止外界创建单例类的对象；
>           - 使用类的私有静态指针变量指向类的唯一实例，并用一个公有的静态方法获取该实例。
>       - 懒汉模式:
>           - 在第一次使用的时候才进行初始化。
>       - 饿汉模式:
>           - 在程序运行时立即初始化。
***
##### 懒汉模式
>- 经典的线程安全懒汉模式，使用双检测锁模式。
>- 代码示例:
```C++
class single{
private:
    // 私有静态指针变量指向唯一实例
    static signle *p;

    // 静态锁，因为类的静态函数只能访问静态成员
    static pthread_mutex_t lock;

    // 私有化构造函数
    signle(){
        pthread_mutex_init(&lock,NULL);
    }
    ~signle(){}
public:
    // 公有静态方法获取实例
    static signle* getinstance();
};
// 初始化&函数定义
pthread_mutex_t signale::lock;// 类内静态成员要在外定义
single* single::p = NULL;
single* single::getinstance(){
    if(p == NULL){// 仅在第一次调用获取实例的时候加锁
        pthread_mutex_lock(&lock);
        if(p == NULL){
            p = new single;
        }
        pthread_mutex_unlock(&lock);
    }
    // 第一次调用获取创建实例后，不用加锁了。
    return p;
}
```
>- 为什么要使用双检测锁?
>   - 如果只检测一次，在每次调用获取实例的方法时，都需要加锁，这将严重影响程序性能。
>   - 双层检测可以有效避免这种情况，仅在第一次创建单例的时候加锁，其它时候都不再符合NULL==P情况，直接返回已创建好的实例。
***
##### 局部静态变量之线程安全懒汉模式
>- 上面的双检测锁，不是很优雅，《effective c++》中给出了更优雅的实现。——使用函数内的局部静态对象，这种方法不用加锁和解锁操作。
>- 代码示例:
```C++
class single{
private:
    single(){}
    ~single(){}
public:
    static single* getinstance();
};

single* single::getinstance(){
    static single obj;// 每次进入函数调用都是同一个obj
    return &obj;
}
```
>- C++0X之后，要求编译器保证内部静态变量的线程安全性。
>   - C++0X是C++11标准成为正式标准之前的草案临时名字。
>- 所以，如果使用C++11之前的版本，同样要加锁，如下所示:
```C++
class single{
private:
    static pthread_mutex_t lock;
    single(){
        pthread_mutex_init(&lock,NULL);
    }
    ~single(){}
public:
    static single* getinstance();
};

single* single::getinstance(){
    pthread_mutex_t single::lock;// 类内静态成员要在外定义
    pthread_mutex_lock(&lock);
    static single obj;
    pthread_mutex_unlock(&lock);
    return &obj;
}
```
****
#### 饿汉模式
>- 饿汉模式不需要加锁，就可以实现线程安全。
>   - 因为在程序运行时，就定义了对象，并对其初始化，所以之后不管那个线程调用getinstance()获取示例，都只不过是返回一个对象的指针而已。
>   - 所以是线程安全的，不需要在获取实例的成员函数中加锁。
>- 代码示例:
```C++
class signel{
private:
    static single* p;
    single(){}
    ~single(){}
public:
    static single* getinstance();
};
single* single::p = new single();
single* single::getinstance(){
    return p;
}

// 测试
int main(){
    single *p1 = single::getinstance();
    single *p2 = single::getinstance();
    if(p1 == p2)std::cout<<"same"<<std::endl;
    return 0;
}
```
>- 饿汉模式虽好，但是存在隐藏的问题，在于非静态对象(函数外的static对象)在不同编译单元中的初始化顺序是未定义的。如果在初始化完成之前调用getinstance()方法会返回一个未定义的实例。
***
#### 生产者-消费者模型
>- **生产者-消费者模型**
>   - 并发编程中经典模型，以多线程为例，为了实现线程间的数据同步，生产者线程与消费者线程共享一个缓冲区。
>       - 一个线程往里面放(push)数据——即生产者
>       - 一个线程从里面拿(pop)数据——即消费者
***
##### 信号量
>- pthread_cond_wait函数，用于等待目标条件变量，该函数调用时需要传入mutex参数(加锁的互斥锁)，函数执行成功时，先把调用线程放入条件变量的请求队列中，然后将互斥锁mutex解锁，当函数成功返回为0时，表示重新抢到了互斥锁，互斥锁会再次被锁上，也就是说函数内部会有一次解锁和加锁操作。
>- `多线程通信时，通过信号量来协调任务，信号量值是多少，当前就有多少可用任务。`
>- pthread_cond_wait方式如下:
```C++
pthread _mutex_lock(&mutex)
while(线程执行的条件是否成立){
    pthread_cond_wait(&cond, &mutex);
}
pthread_mutex_unlock(&mutex);
```
>- pthread_cond_wait执行后的内部操作分以下几步:
>   - `将线程放在条件变量的请求队列后，内部解锁`(在之前先加锁)。
>   - 线程等待被pthread_cond_broadcast信号唤醒或pthread_cond_signal信号唤醒，唤醒后去竞争锁。
>   - 若竞争到互斥锁，内部`再次加锁`。(然后手动解锁)
>   - 相关补充——[【操作系统】线程的使用](https://banshengua.top/%e3%80%90%e6%93%8d%e4%bd%9c%e7%b3%bb%e7%bb%9f%e3%80%91%e7%ba%bf%e7%a8%8b%e7%9a%84%e4%bd%bf%e7%94%a8/)
***
>- **使用前为什么要加锁？**
>   - 多线程访问，为了避免资源竞争，所以要加锁，使得每个线程互斥访问共有资源。
>- **pthread_cond_wait内部为什么要解锁?**
>   - 如果while/if满足执行条件，线程便会调用pthread_cond_wait阻塞自己，此时它还在持有锁，如果它不解锁，那么其他线程将会无法访问公有资源。
>- **为什么要把调用线程放入条件变量的请求队列后再解锁？**
>   - 并发执行的线程，如果把A放在等待队列之前，就释放了互斥锁，其它线程就可以去访问共有资源，这时候线程A所等待的条件改变了，但是没有被放在等待队列上，导致A忽略了等待条件被满足的信号。
>   - 若在线程A调用pthread_cond_wait开始，到把A放在等待队列的过程中，都持有互斥锁，其他线程就无法得到互斥锁，就不能改变公有资源。
***
>- `貌似: 条件变量的目的是等待某种条件，然后把对应的线程放在等待队列中。结合循环/条件判断使用，`详见下方示例代码。
***
>- **为什么判断线程执行的条件用while而不是if?**
>   - 在多线程资源竞争的时候，在一个使用资源的线程里面(消费者)判断资源是否可用，如果不可用，便调用pthread_cond_wait,在另一个线程里面(生产者)如果判断资源可用的话，则调用pthread_cond_signal发送一个资源可用信号。
>- 在wait成功之后，资源就一定可以被使用了吗?不。
>   - 如果同时有两个或两个以上的线程正在等待此资源，wait返回后，资源可能已经被使用了。
>- 再具体说，有可能有多个线程都在等待这个资源可用的信号，信号发出后只有一个资源可用，但是A,B两个线程都在等待。
>   - B比较快，先获得互斥锁，然后加锁，消耗资源，然后解锁。
>   - A之后获得互斥锁，这时候A发现资源已经被使用了，它有两个选择，去访问不存在的资源，或是，继续等待，继续的等待的话，就是使用while,否则使用pthread_cond_wait返回后，就会顺序执行下去。
>- 当只有一个消费者的时候，使用if是OK的。
***
##### 生产者-消费者模型
>- 生产者和消费者是互斥关系，两个对缓冲区访问互斥，同时生产者和消费者又是一个相互协作与同步关系，只有生产者生产之后，消费者才能消费。
>- 代码示例: 来自《Unix环境高级编程》
>   - process_msg相当于消费者
>   - enqueue_msg相当于生产者
>   - struct msg* workq作为缓冲队列
```C++
#include <pthread.h>
struct msg{
    struct msg* m_next;
    // ... ... 
};

struct msg* workq;
pthread_cond_t qready = PTHREAD_COND_INITIALIZER;// 初始化
pthread_mutex_t qlock = PTHREAD_MUTEX_INITIALIZER;// 初始化

void process_msg(){// 消费者
    struct msg* mp;
    for(;;){
        pthread_mutex_lock(&qlock);
        // 注意这里使用while
        while(workq == NULL){
            pthread_cond_wait(&qread,&qlock);
        }
        mq= workq;
        workq = mp->m_next;
        pthread_mutex_unlock(&qlock)

    }
}
void enqueue_msg(struct msg*mp){// 生产者
    pthread_mutex_lock(&qlock);
    mp->m_next = workq;// 头插
    wordp = mp;
    pthread_mutex_unlock(&qlock);
    // 此时另外一个线程在signal之前，执行了process_msg,刚好把mp元素拿走
    pthread_cond_signal(&qready);
    // 此时执行singal，在pthread_cond_wait等待线程被唤醒。
    // 但是mp元素已经被另外一个线程拿走，所以，workq还是NULL,因此需要继续的等待。
}
```
#### 异步模式
>- **异步模式**
>   - 将要缩写的日志内容先存入阻塞队队列，写线程从中取出然后写入。
***
### log
#### 相关API
##### fputs
>- fputs: 将数据写入字符串流。
```C++
#include <stdio.h>
int fputs(const char* st,FILE* stream);
```
>- str: 一个数组包含了写入的以空字符终止的字符序列。
>- stream: 指向FILE对象的指针，该FILE对象标识了要被写入字符串的流。
***
##### 可变参数宏
>- 可变参数宏__VA_ARGS__
>- 定义宏时参数列表的最后一个参数为省略号，在实际使用时会发有时会加## 有时不加。
>- 代码示例
```C++
#define my_print1(...) printf(__VA_ARGS__)

#define my_print2(format,...)printf(format,__VA_ARGS__)
#define my_print3(format,...)printf(format,##__VA_ARGS__)
```
>- __VA_ARGS__宏前面的##作用在于，当可变参数的个数为0时，这里printf参数列表中的##会把前面多余的","去掉，否则会编译出错，建议使用，从而使得程序更加健壮。
***
##### fflush
```C++
#include <stdio.h>
int fflush(FILE* stream);
```
>- fflush会强迫缓冲区内的数据写到参数stream指定的文件中，如果参数stream为NULL，fflush()会将所有打开的文件数据更新。
>- 在使用多个输出函数进行多次输出到控制台时，有可能下一个数据,再上一个数据还没输出完毕，还在输出缓冲区的时候，就被printf加入到输出缓冲区，覆盖率原来的数据，出现输出错误。
>- 在printf()后加上fflush(stdout)，强制马上输出到控制台，可以避免出现上述错误。
***
#### 流程图
![](https://mmbiz.qpic.cn/mmbiz_jpg/6OkibcrXVmBEOjicsa8vpoLAlODicrC7AoM1h2eq9sDMdQY8TNYQoVckCRDd0m8SDH1myuB4gEJfejvznfZuJ3cpQ/640?wx_fmt=jpeg&wxfrom=5&wx_lazy=1&wx_co=1)
>- 日志文件:
>   - 局部变量的懒汉模式获取实例
>   - 生成日志文件，并判断同步和异步写入方式
>- 同步:
>   - 判断是否分文件
>   - 直接格式化输出内容，将信息写入日志文件
>-  异步:
>   - 判断是否分文件
>   - 格式化输出内容，将内容写入阻塞队列，创建一个写线程，从阻塞队列中取出内容写入日志文件。
***
#### 日志类
>- 使用局部变量的懒汉单例模式，创建日志实例，初始化生成日志文件后，格式化输出内容，并根据不同的写入方式执行对应的逻辑。

>- 具体内容，详见代码中的注释。

#### 补充:
>- [va_list使用](https://www.cnblogs.com/qiwu1314/p/9844039.html)


***
### sql_connect
#### 池
>- 池是一组资源的集合，这组资源在服务器启动之初就被完全创建好并初始化。
>- 即，池是资源的容器，本质上是对资源的复用。
***
>- 什么是数据库连接池?
>   - 即，连接池中的资源为一组数据库连接，由程序动态地对池中的连接进行使用，释放。
>   - 当系统开始处理客户请求的时候，如果它需要相关的资源，可以直接从池中获取，无序动态分配；
>   - 当服务器处理完一个客户连接后，可以把相关的资源放回池中，无需执行系统调用释放资源。
>- 数据库访问的一般流程:
>   - 系统创建数据库连接
>   - 完成数据库操作
>   - 断开数据库连接
>- 为什么要创建连接池?
>   - 如果系统需要频繁的访问数据库，则需要频繁的创建与断开。
>   - 创建数据库连接是一个很耗时的操作，也容易对数据库造成安全隐患。
>   - 在程序初始化的时候，集中创建多个数据库连接，并把他们集中管理，供程序使用，可以保证较快的数据库读写速度，更加安全可靠。
***
>- 本项目中，使用单例模式和链表创建数据库连接池，实现对数据库资源的复用。
>- 数据库模块分为两个部分:
>   - 数据库连接池的定义 
>   - 利用连接池完成登录和注册的校验功能
>- 具体使用: 工作线程从数据库连接池中取得一个连接，访问数据库中的数据，访问完成后将连接交连接池。
>- 涉及到的具体内容:
>   - 单例模式创建
>   - 连接池代码实现
>   - RAII机制释放数据库连接
***
### thread_pool
>- 线程池
>   - 空间换时间，消耗服务器的硬件资源，换取运行效率。
>   - 池是一组资源的集合，这组资源在服务器启动之初就被完全创建好并初始化，这称为静态资源。
>   - 当服务器进入正式运行阶段，开始处理客户请求的时候，如果它需要相关的资源，可以直接从池中获取，无需动态分配。
>   - 当服务器处理完一个客户连接后，可以把相关的资源放回池中，无需执行系统调用释放资源。
***
#### pthread_create
>- 该函数的第三个参数，为函数指针，指向处理线程函数的地址。
>   - 该处理函数要求为静态函数，如果为类成员函数时，需要将其设置为`静态成员函数`。
>   - 因为如果为普通类成员函数，this指针会作为默认的参数被传进函数中，从而和线程函数参数void*不匹配，无法通过编译。
>   - 静态成员没事，因为静态成员函数没有this指针，静态成员函数只能访问静态成员。
***
#### 线程池分析
>- 本项目线程池的设计模式为半同步/半反应堆，其中反应堆具体为Poractor事件处理模式。
>- 具体的，主线程为异步线程，负责监听文件描述，接收新的socket连接，若监听的socket发生了读写事件，将任务插入到请求队列。工作线程从请求队列中取出任务，进行逻辑处理。
***
### lst_timer

***
### http_conn
>- epoll内容略。
***
>- 有限状态机-[补充](https://banshengua.top/%e3%80%90socket%e3%80%91%e6%9c%89%e9%99%90%e7%8a%b6%e6%80%81%e6%9c%ba/)
>   - 有限状态机，是一种抽象的理论模型，它能够把有限个变量描述的状态变化过程，以可构造可验证的方式呈现出来。比如: 封闭的有向图
>   - 有限状态机可通过if-else,switch-case和函数指针来实现，从软件工程的角度看，主要是为了封装逻辑。