#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include "../lock/locker.h"
#include "../common/common.h"


// 阻塞(消息)队列类
template<typename T>
class block_queue{

public:
    block_queue(int max_size = 100);// 构造
    ~block_queue();

    void clear();// 置空

    bool full();// 判断队列是否满了

    bool empty();// 判断队列是否为空

    bool front(T &value);// 返回队首元素

    bool back(T &value);// 返回队尾元素

    int size();// 当前元素个数

    int max_size();// 最大容量

    // 往队列中添加元素，需要将所有使用对列的线程先唤醒
    // 当有元素push进队列，相当于生产者生产了一个元素
    // 若当前没有线程等待条件变量，则唤醒无意义
    bool push(const T & item);// 往队列中添加元素

    // pop时，如果当前队列没有元素，将会等待条件变量
    bool pop(T &item);// 取出一个元素

    // 增加超时处理的消费者
    // 项目中并没有使用到;
    // 在pthread_cond_wait基础上增加了等待的时间;
    // 指的是在指定时间内能抢到互斥锁即可
    // 其他逻辑不变
    bool pop(T &item,int ms_timeout);

private:
    locker m_mutex;
    cond m_cond;

    T* m_array;     // 消息队列         
    int m_size;     // 当前消息长度
    int m_max_size; // 最大长度
    int m_front;    // 首元素index
    int m_back;     // 尾元素index
};

#endif