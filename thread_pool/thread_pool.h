#ifndef THREAD_POOL
#define THREAD_POOL

#include "../common/common.h"
#include "../sql_connection_pool/sql_connection_pool.h"

template <typename T>
class thread_pool{

public:
    /*有参构造
    - 使用的模型
    - 数据库连接池
    - 线程数量-默认8
    - 最大允许等待处理的任务数量-默认10000*/
    thread_pool(int actor_model,
                connection_pool *connPool,
                int thread_number = 8,
                int max_request = 10000);

    ~thread_pool();

    // 往请求队列中插入任务
    bool append(T* request,int state);

    bool append_p(T* request);

private:
    // 工作线程运行的函数，它不断从工作队列中取出任务并执行
    static void worker(void* arg);

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

#endif