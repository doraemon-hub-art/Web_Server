#ifndef WEBSERVER_H
#define WEBSERVER_H


#include "../common/common.h"
#include "../thread_pool/thread_pool.h"
#include "../http_conn/http_conn.h"

const int MAX_FD = 65535;   // 最大文件描述符
const int MAX_EVENT_NUMBER = 10000;// 最大事件数
const int TIMESLOT = 5;         // 最小超时时间

class WebServer{
public:
    WebServer();
    ~WebServer();

    // 初始化
    void init(int port, 
              std::string user,
              std::string passwd,
              std::string databaseName,
              int log_write,
              int opt_linger,
              int trigmode,
              int sql_num,
              int thread_num,
              int close_log,
              int actor_mode){
    }

    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void eventListen();
    void eventLoop();
    void timer(int connfd,struct sockaddr_in client_address);
    void adjust_timer(int confd,struct sockaddr_in client_address);
    void deal_timer(util_timer *timer,int sockfd);
    bool deal_client_data();
    bool deal_with_signal(bool& timerout,bool& stop_server);
    bool deal_with_read(int sockfd);
    void deal_with_write(int sockfd);
private:

    int m_port;
    char* m_root;
    int m_log_write;
    int m_close_log;
    int m_actor_model;

    int m_pipefd[2];
    int m_epollfd;
    http_conn *users;

    // 数据库相关
    connection_pool *m_connPool;
    std::string m_user;// 用户名
    std::string m_passwd;// 密码
    std::string data_base_Name;// 数据库名
    int m_sql_num;

    //线程池相关
    thread_pool<http_conn> *m_pool;

};

#endif