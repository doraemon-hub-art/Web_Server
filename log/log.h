#ifndef LOG_H
#define LOG_H

#include "../common/common.h"
#include "../block_queue/block_queue.h"

class Log{

public:
    // C++11之后，局部静态变量线程安全
    static Log* get_instance(){
        static Log instance;
        return &instance;
    }
    
    /*初始化-可选择的参数: 
         日志文件
         日志缓冲区大小
         最大行数
         最长日志条队列(阻塞队列长度)
    完成日志的创建，写入方式的判断。
    */
   bool init(const char* file_name,
             int close_log,
             int log_buf_size = 8192,
             int split_lines = 5000000,
             int max_queue_size = 0);

    // 异步写日志共有方法
    // 调用私有方法async_write_log
    static void* flush_log_thread(void* args){
        Log::get_instance()->async_write_log();
    }

    // 将输出内容按照标准格式整理
    // 完成写入日志文件中的具体内容，主要实现，日志分级、分文件、格式化输出内容。
    void write_log(int level,const char* format,...);

    // 强制刷新缓冲区
    void flush(void);

private:
    Log();
    virtual ~Log();

    // 异步写日志方法
    void *async_write_log(){
        std::string single_log;
    
        // 从阻塞队列中取出一条日志内容，写入文件
        while(m_log_queue->pop(single_log)){
            m_mutex.lock();
            fputs(single_log.c_str(),m_fp);
            m_mutex.unlock();
        }
    }

private:
    char dir_name[128]; // 路径名
    char log_name[128]; // log文件名
    int m_split_lines;  // 日志最大行数
    int m_log_buf_size; // 日志缓冲区大小
    long long m_count;  // 当前日志行数
    int m_today;        // 按天分类，记录当前是哪一天
    FILE* m_fp;         // 打开log的文件指针
    char* m_buf;        // 要输出的内容
    block_queue<std::string>*m_log_queue;    // 阻塞队列
    bool m_is_async;    // 是否使用异步模式写日志 true-异步 false-同步
    locker m_mutex;     // 互斥锁
    int m_close_log; // 关闭日志
};

// 这四个宏定义在其他文件中使用，主要用于不同类型的日志输出
// 可变参数宏
#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}





# endif 