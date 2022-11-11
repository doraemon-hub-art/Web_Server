#ifndef LST_TIMER_H
#define LST_TIMER_H

#include "../common/common.h"
#include "../log/log.h"
#include "../http_conn/http_conn.h"

class util_timer;// 前置声明
struct client_data{// 用户连接信息结构体
    sockaddr_in address;
    int sockfd;
    util_timer *timer;
};

// 定时器链表结点——定时器类(结点)
class util_timer{

public:
    
public:
    time_t expire;// 计时——绝对时间，越大的越靠后
    void (* cb_func)(client_data*);
    client_data* user_data;// 存储信息
    util_timer* prev;
    util_timer* next;
};

// 升序链表
// 定时器链表——结点容器(使用结点组成lianbiao)
class sort_timer_list{

public:
    sort_timer_list();
    ~sort_timer_list(); 

    void add_timer(util_timer *timer);
    void adjust_timer(util_timer* timer);
    void del_timer(util_timer* timer);
    void tick();

private:
    // 私有成员，被共有成员add_timer和adjust_timer调用
    // 主要用于调整链表内部结点
    void add_timer(util_timer* timer,util_timer* list_head);

    util_timer* head;
    util_timer* tail;
};

// 操作定时器链表类
class Utils{
   
public:
    Utils(){}
    ~Utils(){}

    void init(int timeslot);

    // 对文件描述设置非阻塞
    int setnonblocking(int fd);

    // 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    // EPOLLONESHOT——避免多线程产生竞态
    void addfd(int epollfd,int fd,bool one_shot,int TRIGMode);

    // 信号处理函数
    static void sig_handler(int sig);

    // 设置信号函数
    void addsig(int sig,void(handler)(int),bool restart = true);

    // 定时处理任务，重新定时以不断出发SIGALRM信号
    void timer_handler();

    void show_error(int connfd,const char* info);

public:
    static int* u_pipefd;
    sort_timer_list m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};

void cb_func(client_data* user_data);

#endif