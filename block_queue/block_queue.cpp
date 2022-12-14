#include "block_queue.h"

// 函数的默认参数声明时写，定义时不要写。
template<typename T>
block_queue<T>::block_queue(int max_size):
    m_max_size(m_max_size),
    m_size(0),
    m_front(-1),
    m_back(-1),
    m_array(new T[max_size]){
    if(max_size <= 0){
        exit(-1);
    }
}

template<typename T>
block_queue<T>::~block_queue(){
    m_mutex.lock();
    if(m_array != NULL){
        delete[] m_array;
    }
    m_mutex.unlock();
    // 不会主动调用析构，其它变量不用销毁或者置空
}

template<typename T>
void block_queue<T>::clear(){
    m_mutex.lock();
    m_size = 0;
    m_front = -1;
    m_back = -1;
    m_mutex.unlock();
}

template<typename T>
bool block_queue<T>::full(){
    m_mutex.lock();
    if(m_size >= m_max_size){
        m_mutex.unlock(); 
        return true;
    }
    m_mutex.unlock();
    return false;
}

template<typename T>
bool block_queue<T>::empty(){
    m_mutex.lock();
    if(m_size == 0){
        m_mutex.unlock();
        return true;
    }
    m_mutex.unlock();
    return false;
}

template<typename T>
bool block_queue<T>::front(T &value){
    m_mutex.lock();
    if(m_size == 0){
        m_mutex.unlock();
        return false;
    }
    value = m_array[m_front];
    m_mutex.unlock();
    return true;
}

template<typename T>
bool block_queue<T>::back(T &value){
    m_mutex.lock();
    if(m_size == 0){
        m_mutex.unlock();
        return false;
    }
    value = m_array[m_back];
    m_mutex.unlock();
    return true;
}

template<typename T>
int block_queue<T>::size(){
    int tmp = 0;
    m_mutex.lock();
    tmp = m_size;
    m_mutex.unlock();
    return tmp;
}

template<typename T>
int block_queue<T>::max_size(){
    int tmp = 0;
    m_mutex.lock();
    tmp = m_max_size;
    m_mutex.unlock();
    return tmp;
}

template<typename T>
bool block_queue<T>::push(const T &item){
    m_mutex.lock();
    if(m_size >= m_max_size){// 满了，唤醒线程赶紧处理
        m_cond.broadcast();
        m_mutex.unlock();
        return false;
    }
    // m_back初始值是-1，所以要+1才是真正的位置
    // % 用于队列循环
    m_back = (m_back+1)%m_max_size;
    m_array[m_back] = item;
    m_size++;
    m_cond.broadcast();// 唤醒消费者来处理
    m_mutex.unlock();
    return true;
}

template<typename T>
bool block_queue<T>::pop(T &item){
    m_mutex.lock();
    // 循环等待有可用任务
    while (m_size <= 0){    
        if(!m_cond.wait(m_mutex.get())){
            m_mutex.unlock();
            return false;
        }
    }
    // 取出来
    item = m_array[m_front];
    // 同上，因为m_front初识-1
    m_front = (m_front+1)%m_max_size;
    m_size--;
    m_mutex.unlock();
    return false;
}

template<typename T>
bool block_queue<T>::pop(T &item,int ms_timeout){
    // 高级用法花括号{}初始化
    struct timespec t = {0,0};
    struct timeval now = {0,0};
    // 获取当前时间存到now中
    gettimeofday(&now,NULL);
    m_mutex.lock();
    if(m_size <= 0){// 没有可用任务，尝试等待
        t.tv_sec = now.tv_sec + ms_timeout / 1000;
        t.tv_nsec = (ms_timeout % 1000) * 1000;
        if(!m_cond.timewait(m_mutex.get(),t)){
            m_mutex.unlock();
            return false;
        }
    }

    if(m_size <= 0){
        m_mutex.unlock();
        return false;
    }

    m_front = (m_front + 1) % m_max_size;
    item = m_array[m_front];
    m_size--;
    m_mutex.unlock();
    return true;
}

