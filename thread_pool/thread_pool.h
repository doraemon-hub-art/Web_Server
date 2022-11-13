#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "../common/common.h"
#include "../sql_connection_pool/sql_connection_pool.h"
#include "../lock/locker.h"

template <typename T>
class threadpool{
public:
    /*有参构造
    - 使用的模型
    - 数据库连接池
    - 线程数量-默认8
    - 最大允许等待处理的任务数量-默认10000*/
    threadpool(int actor_model,
                connection_pool *connPool,
                int thread_number = 8,
                int max_request = 10000);

    ~threadpool();

    // 往请求队列中插入任务
    bool append(T* request,int state);

    bool append_p(T* request);

private:
    // 工作线程运行的函数，它不断从工作队列中取出任务并执行
    static void* worker(void* arg);

    // 执行任务——从工作线程取出某个任务进行处理
    void run();

private:

    int m_thread_number; // 线程池中的线程数
    int m_max_requests; // 请求队列中允许的最大请求数
    pthread_t *m_threads; // 线程数组-线程池，大小为m_thread_number;
    std::list<T*>m_worker_queue; // 请求队列
    locker m_mutex; // 互斥锁
    sem m_queue_stat; // 信号量-是否有任务需要处理
    connection_pool *m_connPool; // 数据库连接池
    int m_actor_model; // 模型切换
};

template<typename T>
threadpool<T>::threadpool(int actor_model,
                            connection_pool *connPool,
                            int thread_number,
                            int max_request):
        m_actor_model(actor_model),
        m_thread_number(thread_number),
        m_max_requests(max_request),
        m_threads(NULL),
        m_connPool(connPool){
    if(thread_number <= 0 || max_request <= 0){// 参数不合法
        throw std::exception();
    }
    m_threads = new pthread_t[m_thread_number];// 开辟存储线程标识的数组
    if(m_threads == NULL){// new 失败
        throw std::exception();
    }
    for(int i = 0; i < thread_number; i++){
        // pthread_t*类型数组，+1即移动sizeof(pthread_t)字节,移动到下一个成员处。
        if(pthread_create(m_threads + i,NULL,worker,this) != 0){// 如果创建失败
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])){// 与主线程分离，自己释放资源
            delete[] m_threads;
            throw std::exception();
        }
    }

}


template<typename T>
threadpool<T>::~threadpool(){
    delete[] m_threads;
}

template<typename T>
bool threadpool<T>::append(T* request,int state){
    m_mutex.lock();
    if(m_worker_queue.size() >= m_max_requests){// 请求队列已经满啦
        m_mutex.unlock();
        return false;
    }
    request->m_state = state;
    m_worker_queue.push_back(request);
    m_mutex.unlock();
    m_queue_stat.post();
    return true;
}

template<typename T>
bool threadpool<T>::append_p(T* request){
    m_mutex.lock();
    if(m_worker_queue.size() >= m_max_requests){
        m_mutex.unlock();
        return false;
    }
    m_worker_queue.push_back(request);
    m_mutex.unlock();
    m_queue_stat.post();
    return true;
}

template<typename T>
void* threadpool<T>::worker(void *arg){
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run(){
    while(true){
        m_queue_stat.wait();// 看是否有可用任务
        m_mutex.lock();// 上锁
        if(m_worker_queue.empty()){// 空的-为什么会空，因为有可能会被其它线程抢先一步(等待队列中靠后)
            m_mutex.unlock();
            continue;
        }
        T* request = m_worker_queue.front();
        m_worker_queue.pop_front();
        // 拿到后开锁
        m_mutex.unlock();
        if(!request)continue;// 空的

        if(m_actor_model == 1){
            if(request->m_state == 0){
                if(request->read_once()){// 读，tm这个request是个什么jb东西，怎么没说啊。
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql,m_connPool);// 获取数据库连接
                }else{
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }else{
            if(request->write()){// 写
                request->improv = 1;
            }else{
                connectionRAII mysqlcon(&request->mysql,m_connPool);
                request->process();
            }
        }

    }
}

#endif