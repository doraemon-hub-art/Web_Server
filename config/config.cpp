#include "config.h"


Config::Config(){
    m_port = 9006;  // 设置默认端口
    m_log_wirte = 0;    // 日志写入方式
    m_trig_mode = 0;    // 触发组合模式，默认为listen_fd LT水平触发 + connect_fd LT水平触发
    m_listen_trig_mode = 0; // listen_fd默认触发 LT
    m_connect_trig_mode = 0;    // connect_fd默认触发 LT
    m_opt_linger = 0; // 默认不使用优雅关闭链接
    m_sql_num = 8;    // 数据库连接池数量，默认8
    m_thread_num = 8; // 线程池内线程数量，默认8
    m_close_log = 0;  // 关闭日志，默认不关闭
    m_actor_model = 0;// 事件处理模式选择，默认为proactor

}

void Config::parse_arg(int argc,char* argv[]){
    int opt;
    const char *str = "p:l:m:o:s:t:c:a:";// 用于getopt解析命令行参数
    // 解析参数
    while((opt = getopt(argc,argv,str)) != -1){
        switch (opt){
        case 'p':
            m_port = atoi(optarg);
            break;
        case 'l':
            m_log_wirte = atoi(optarg);
            break;
        case 'm':
            m_trig_mode = atoi(optarg);
            break;
        case 'o':
            m_opt_linger = atoi(optarg);
            break;
        case 's':
            m_sql_num = atoi(optarg);
            break;
        case 't':
            m_thread_num = atoi(optarg);
            break;
        case 'c':
            m_close_log = atoi(optarg);
            break;
        case 'a':
            m_actor_model = atoi(optarg);
            break;
        default:
            break;
        }
    }
}