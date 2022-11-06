#include "log.h"


Log::Log(){
    m_count = 0;
    m_is_async = false;
    // 其他东西都在init()函数中进行初始化
}

Log::~Log(){
    if(m_fp != NULL){
        fclose(m_fp);
    }
}

void Log::flush(void){
    m_mutex.lock();
    fflush(m_fp);// 将缓冲区中的内容写m_fp中
    m_mutex.unlock();
}

bool Log::init(const char *file_name,
               int close_log,
               int log_buf_size,
               int split_lines,
               int max_queue_size){
    // 根据阻塞队列长的来设置异步 or 同步
    // 阻塞队列的最大长度为0，说明为同步。
    if(max_queue_size >= 1){// 异步
        m_is_async = true;// 设置为异步写入方式

        // 创建阻塞队列并设置长度
        m_log_queue = new block_queue<std::string>(max_queue_size);

        // 创建一个线程用来异步写日志，执行flush_log_thread函数。
        pthread_t tid;
        pthread_create(&tid,NULL,flush_log_thread,NULL);
    }

    m_close_log = close_log;
    // 输出内容的长度
    m_log_buf_size = log_buf_size;// 设置日志缓冲区的大小
    m_buf = new char[m_log_buf_size];// 开辟空间
    memset(m_buf,'\0',sizeof(m_buf));

    // 日志的最大行数
    m_split_lines = split_lines;// 设置日志文件的行数

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    // 从后往前找第一个 /  的位置
    const char *p = strrchr(file_name,'/');
    char log_full_name[256] = {0};// 最终的文件名——时间等信息+指定的名字

    // 获取文件名和文件夹名
    // 若输入的文件名中没有 / 则直接将时间+文件名作为日志名
    if( p == NULL){
        snprintf(log_full_name,255,
                "%d_%02d_%02d_%s",
                my_tm.tm_year+1900,
                my_tm.tm_mon+1,
                my_tm.tm_mday,
                file_name);
    }else{
        // 将 / 的位置向后移动一个位置，然后赋值到logname中
        // p -file_name + 1是文件所在路径文件夹的长度
        // 名字拷贝到log_name中，p+1将字符串指向最后一个/后的字符
        strcpy(log_name,p+1);
        // 字符指针相爱相减，相当于计算长度，p位置-file_name(字符串开头)，然后+1
        strncpy(dir_name,file_name,p-file_name+1);// 
        // 后面的参数跟format有关
        snprintf(log_full_name,255,
                "%s%d_%02d_%02d_%s",
                dir_name,
                my_tm.tm_year+1900,// 自1900年起的年数
                my_tm.tm_mon+1,
                my_tm.tm_mday,
                log_name);
    }
    m_today = my_tm.tm_mday;

    // 写入方式打开，不存在则创建
    m_fp = fopen(log_full_name,"a"); 
    if(m_fp == NULL){
        return false;
    }
    return true;
}

// 写入文件
/*
    - Debug: 调试代码的输出
    - Warn: 警报，调试时使用
    - Info: 报告系统当前的状态，当前执行的流程或接收的信息等
    - Error&Fatal: 输出系统的错误信息(本项目中不包括)
*/
void Log::write_log(int level,const char* format,...){// 
    struct timeval now = {0,0};
    gettimeofday(&now,NULL);//  获取当前时间,1970年1月1号00:00到今天
    time_t t =now.tv_sec;
    struct tm* sys_tm = localtime(&t);// 解析填充到sys_tm中
    struct tm my_tm = *sys_tm;

    char s[16] = {0};// 日志前缀

    // 日志分级
    switch (level)
    {
    case 0:
        strcpy(s,"[debug]:");
        break;
    case 1:
        strcpy(s,"[info]:");
        break;
    case 2:
        strcpy(s,"[warn]:");
        break;
    case 3:
        strcpy(s,"[error]:");
        break;
    default:
        stpcpy(s,"[info]:");
        break;
    }

    m_mutex.lock();

    m_count++;// 行数更新

    // 日志不是今天或写入的日志行数是最大行的倍数(超过最大行数)
    // 加完这条才超的，所以这条还可以加上
    if(m_today != my_tm.tm_mday || m_count % m_split_lines == 0){
        char new_log[256] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};

        snprintf(tail,16,
                "%d_%02d_%02d_",
                my_tm.tm_year + 1900,
                my_tm.tm_mon + 1,
                my_tm.tm_mday);
        
        if(m_today != my_tm.tm_mday){// 不是今天
            snprintf(new_log,255,"%s%s%s",dirname,tail,log_name);
            m_today = my_tm.tm_mday;// 更新当天
            m_count = 0;
        }else{// 是今天但是满了
            snprintf(new_log,255,"%s%s%s.%lld",dir_name,tail,log_name,m_count/m_split_lines);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        m_fp = fopen(new_log,"a");// 创建新的文件
    }

    m_mutex.unlock();

    va_list valst;  // 指向可变参数的指针
    va_start(valst,format);// 初始化valst，使其指向第一个可变参数的地址。

    std::string log_str;
    m_mutex.lock();
    // 写入的具体时间内容格式
    int n = snprintf(m_buf, 48, 
                    "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, 
                     my_tm.tm_mon + 1, 
                     my_tm.tm_mday,
                     my_tm.tm_hour, 
                     my_tm.tm_min, 
                     my_tm.tm_sec, 
                     now.tv_usec, s);
    // 时间信息+日志内容汇总到m_buf中
    // 可能format默认格式就是全打印出来吧。
    int m = vsnprintf(m_buf+n,m_log_buf_size-1,format,valst);
    m_buf[n+m] = '\n';// 换行符
    m_buf[n+m+1] = '\0';// 结束符
    log_str = m_buf;

    m_mutex.unlock();

    if(m_is_async && !m_log_queue->full()){// 异步存储，并且阻塞队列没满
        m_log_queue->push(log_str);
    }else{
        m_mutex.lock();
        fputs(log_str.c_str(),m_fp);
        m_mutex.unlock();
    }
    va_end(valst);
}