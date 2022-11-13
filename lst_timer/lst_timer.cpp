#include "lst_timer.h"

sort_timer_list::sort_timer_list() {
    head = NULL;
    tail = NULL;
}

// 释放定时器链表
sort_timer_list::~sort_timer_list() {
    util_timer *tmp = head;
    while(tmp){
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

// 添加定时器
void sort_timer_list::add_timer(util_timer *timer) {
    if(!timer){// 空的
        return ;
    }
    if(!head){// 第一次插入
        head = tail = timer;
    }
    if(timer->expire < head->expire){
        timer->next = head;
        head->prev = timer;
        head = timer;
        return ;
    }
    add_timer(timer,head);
}

// 任务发生变化时，调整结点在链表中的位置
void sort_timer_list::adjust_timer(util_timer *timer) {
    if(!timer){
        return ;
    }
    util_timer* tmp = timer->next;
    // 下一个结点为空，说明是最后一个结点了,终止
    // 如果已经比下一个结点的定时小了，终止
    if(!tmp || (timer->expire < tmp->expire)){
        return ;
    }
    if(timer == head){// 为头结点
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer,head);// 重新添加，在添加的时候排序
    }else{// 非头结点
        // 从链表中去掉timer结点，拿出来重新添加
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer,timer->next);
    }
}

// 删除定时器
void sort_timer_list::del_timer(util_timer *timer) {
    if(!timer)return;
    if((timer == head) && (timer == tail)){// 就这么一个结点
        delete timer;
        head = NULL;
        tail = NULL;
        return ;
    }

    // 多个结点

    if(timer == head){// 头结点
        head = head->next;
        head->prev = NULL;
        delete timer;
        return ;
    }

    if(timer == tail){// 尾结点
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return ;
    }

    // 中间的结点
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

// 定时任务处理函数
/*
 * 遍历定时器升序链表容器，从头结点开始依次处理每个定时器，直到遇到尚未到期得定时器。
 * 若当前时间小于定时器超时时间，跳出循环，即未找到到期的定时器。
 * 若当前时间大于定时器超时时间，即找到了到期的定时器，执行回调函数，然后将它从链表中删除，然后继续遍历。*/
void sort_timer_list::tick() {// 检查是否有时间到了该执行的任务
    if(!head){// 空的
        return;
    }

    // 获取当前时间
    time_t  cur = time(NULL);
    util_timer* tmp = head;

    // 遍历定时器链表
    while(tmp){
        // 链表容器为升序列表
        // 当前时间小于定时器的超时时间，后面的定时器也没有到期，因为是链表是升序排列的
        if(cur < tmp->expire){
            break;
        }

        // 当前定时器到期，则调用回调函数，执行定时事件。
        tmp->cb_func(tmp->user_data);
        // 将处理后的定时器从链表容器中删除，并重置头结点
        // 因为到期肯定在前面发生，因为是升序链表。
        head = tmp->next;
        if(head){
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

// 调整链表内部结点顺序
void sort_timer_list::add_timer(util_timer *timer, util_timer *list_head) {
    util_timer* prev = list_head;
    util_timer* tmp = prev->next;
    // 遍历当前结点之后的链表，按照超时时间找到目标定时器对应的位置，常规双向链表插入。
    while(tmp){
        if(timer->expire < tmp->expire){
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    // 遍历完发现，目标定时器需要放到尾结点处，在前面没找到合适的插入位置。
    if(!tmp){
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

void Utils::init(int timeslot) {
    m_TIMESLOT = timeslot;
}

// 对文件描述符设置非阻塞
int Utils::setnonblocking(int fd) {
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT，避免竞态。
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
    epoll_event event;
    event.data.fd = fd;

    if(TRIGMode == 1){
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }else{
        event.events = EPOLLIN | EPOLLRDHUP;
    }

    if(one_shot){
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

//  信号处理函数
void Utils::sig_handler(int sig) {
    // 为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1],(char*)&msg,1,0);
    errno = save_errno;
}

// 设置信号函数
void Utils::addsig(int sig, void (*handler)(int), bool restart) {
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler = handler;
    if(restart){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig,&sa,NULL) != -1);// 安装信号，信号-触发后的执行函数
}

// 定时处理任务，重新定时以不断触发SIGALRM信号。
void Utils::timer_handler() {
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}

// 返回错误提示
void Utils::show_error(int connfd, const char *info) {
    send(connfd,info, strlen(info),0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
void cb_func(client_data* user_data){
    // 删除非活动连接在epoll上注册的事件
    epoll_ctl(Utils::u_epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
    assert(user_data);
    close(user_data->sockfd);// 关闭socket
    http_conn::m_user_count--;// 连接数--
}


