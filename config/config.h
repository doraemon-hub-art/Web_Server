#ifndef CONFIG_H
#define CONFIG_H

#include "../common/common.h"

class Config{

public:
    Config();
    ~Config();
    void parse_arg(int argc,char* argv[]);

    int m_port;                 // 端口号
    int m_log_wirte;            // 日志写入方式
    int m_trig_mode;            // 触发组合模式
    int m_listen_trig_mode;     // listenfd-监听socket触发模式
    int m_connect_trig_mode;    // connfd-连接socket触发模式
    int m_opt_linger;             // 优雅关闭连接
    int m_sql_num;                // 数据库连接池数量
    int m_thread_num;             // 线程池内线程数量
    int m_close_log;              // 是否关闭日志
    int m_actor_model;            // 事件处理模式
};


#endif