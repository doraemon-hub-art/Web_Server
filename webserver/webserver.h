#ifndef WEBSERVER_H
#define WEBSERVER_H


#include "../common/common.h"

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

    
private:

}



#endif