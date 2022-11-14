#include "webserver.h"

WebServer::WebServer() {

    // http_conn对象
    users = new http_conn[MAX_FD];

    // root文件夹路径
    char server_path[200];
    getcwd(server_path,200);// 获取当前工作目录的绝对路径
    char root[6] = "/root";
    m_root = (char*) malloc(strlen(server_path) + strlen(root)+1);
    strcpy(m_root,server_path);
    strcat(m_root,root);// 追加

    // 定时器-按照最大的客户端描述符创建
    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer() {
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);

    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

// 初始化
void WebServer::init(int port, std::string user, std::string passwd, std::string databaseName, int log_write,
                     int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_mode) {
    m_port = port;
    m_user = user;
    m_passwd = passwd;
    m_data_base_name =databaseName;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_close_log = close_log;
    m_actor_model = actor_mode;
}

// 触发模式——监听socket+连接socket
void WebServer::trig_mode() {

    if(m_TRIGMode == 0) { // LT+LT
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    }else if(m_TRIGMode == 1) { // LT+ET
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }else if(m_TRIGMode == 2){// ET+LT
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }else if( m_TRIGMode == 3){// ET+ET
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}

// 日志
void WebServer::log_write() {
    if(m_close_log == 0){
        // 初始化日志
        if(m_log_write == 1){
            Log::get_instance()->init("./ServerLog",m_close_log,2000,800000,800);
        }else{
            Log::get_instance()->init("/ServerLog",m_close_log,2000,800000,0);
        }
    }
}

// 数据库连接池
void WebServer::sql_pool() {
    // 初始化数据库连接池
    m_connPool = connection_pool::GetInstance();
    m_connPool->init("localhost",m_user,m_passwd,m_data_base_name,3306,m_sql_num,m_close_log);

    // 初始化数据库读取表
    users->initmysql_result(m_connPool);
}

// 线程池
void WebServer::thread_pool() {
    // 创建线程池
    m_pool = new threadpool<http_conn>(m_actor_model,m_connPool,m_thread_num);

}

// 监听
void WebServer::eventListen() {
    // 网络编程基础步骤
    m_listenfd = socket(PF_INET,SOCK_STREAM,0);
    assert(m_listenfd >= 0);

    // 优雅关闭连接
    if(m_OPT_LINGER == 0){
        struct linger tmp = {0,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }else if(m_OPT_LINGER == 1){
        struct  linger tmp = {1,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    setsockopt(m_listenfd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag));
    ret = bind(m_listenfd,(struct sockaddr*)&address,sizeof(address));
    assert(ret>=0);
    ret = listen(m_listenfd,5);
    assert(ret >=0);

    utils.init(TIMESLOT);

    // epoll创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    utils.addfd(m_epollfd,m_listenfd, false,m_LISTENTrigmode);
    http_conn::m_epollfd = m_epollfd;

    ret = socketpair(PF_UNIX,SOCK_STREAM,0,m_pipefd);
    assert(ret != -1);
    utils.setnonblocking(m_pipefd[1]);// 写端设置非阻塞
    utils.addfd(m_epollfd,m_pipefd[0], false,0);

    utils.addsig(SIGPIPE,SIG_IGN);
    utils.addsig(SIGALRM,utils.sig_handler, false);
    utils.addsig(SIGTERM,utils.sig_handler, false);

    alarm(TIMESLOT);

    // 工具类，信号和描述符基础操作
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

void WebServer::timer(int connfd, struct sockaddr_in client_address) {
    users[connfd].init(connfd,client_address,m_root,m_CONNTrigmode,m_close_log,
                       m_user,m_passwd,m_data_base_name);

    // 初始化client_data数据
    // 创建定时器，设置回调函数和超时时间，绑定用户数据，将定定时器添加到链表中。
    // users_timer[connfd]为该连接相关数据
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer* timer = new util_timer;// 创建一个该连接的结构体
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);// 获取当前时间
    timer->expire = cur+3*TIMESLOT;//设置失效时间
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer(timer);// 将该连接的定时器放到时间链表中
}

// 若有数据传输，则将定时器往后延后3个单位
// 并对新的定时器在链表上的位置进行调整。
 void WebServer::adjust_timer(util_timer *timer) {
    time_t  cur = time(NULL);
    timer->expire = cur*3+TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s","adjust timer once");
}

// 删除结点
void WebServer::deal_timer(util_timer *timer, int sockfd) {
    timer->cb_func(&users_timer[sockfd]);// 删除非活动连接在epoll上注册的事件
    if(timer){
        utils.m_timer_lst.del_timer(timer);// 删除定时器
    }
    LOG_INFO("close fd %d",users_timer[sockfd].sockfd);
}

// 新来的链接
bool WebServer::deal_client_data() {
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if(m_LISTENTrigmode == 0){// LT
        int connfd = accept(m_listenfd,(struct  sockaddr*)&client_address,&client_addrlength);
        if(connfd < 0){
            LOG_ERROR("%s:errno is:%d","accept error",errno);
            return false;
        }
        if(http_conn::m_user_count >= MAX_FD){// 连接满了
            utils.show_error(connfd,"Internal server busy");// 给客户端返回，服务器繁忙
            LOG_ERROR("%s","Internal server busy");
            return false;
        }
        timer(connfd,client_address);// 初始化连接socket定时器
    }else{// ET
        while(1){
            int connfd = accept(m_listenfd,(struct sockaddr*)&client_address,&client_addrlength);
            if(connfd < 0){
                LOG_ERROR("%s:errno is:%d","accept error",errno);
                break;
            }
            if(http_conn::m_user_count >= MAX_FD){// 用户数满了
                utils.show_error(connfd,"Internal server busy");
                LOG_ERROR("%s","Internal server busy");
                break;
            }
            timer(connfd,client_address);
        }
        return false;
    }
    return true;
}

// 信号处理
bool WebServer::deal_with_signal(bool &timeout, bool &stop_server) {
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0],signals,sizeof(signals),0);
    if(ret == -1){
        return false;
    }else if(ret == 0){
        return false;
    }else{// 确实读取到数据了
        for(int i = 0; i < ret;i++){
            switch (signals[i]) {
                case SIGALRM:{
                    timeout = true;
                    break;
                }
                case SIGTERM:{
                    stop_server = true;
                    break;
                }
            }
        }
    }
    return true;
}

// 处理读数据
bool WebServer::deal_with_read(int sockfd) {
    util_timer* timer = users_timer[sockfd].timer;

    // reactpr 1
    if(m_actor_model == 1){
        if(timer){
            adjust_timer(timer);
        }

        // 若检测到读事件，将该事件放入请求队列
        m_pool->append(users+sockfd,0);

        while (true){
            if(users[sockfd].improv){
                if(users[sockfd].timer_flag == 1){
                    deal_timer(timer,sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }else{// proactor
        if(users[sockfd].read_once()){
            LOG_INFO("deal with client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            // 若检测到读事件，将该事件放入请求队列
            m_pool->append(users+sockfd,1);

            if(timer){
                adjust_timer(timer);
            }
        }else{
            deal_timer(timer,sockfd);
        }
    }
}

//  处理写事件
void WebServer::deal_with_write(int sockfd) {
    util_timer* timer = users_timer[sockfd].timer;
    // reactor
    if(m_actor_model == 1){
        if(timer){
            adjust_timer(timer);// 往后延长时间
        }
        m_pool->append(users + sockfd,1);
        while(true){
            if(users[sockfd].timer_flag == 1){
                deal_timer(timer,sockfd);
                users[sockfd].improv = 0;
                break;
            }
            users[sockfd].improv = 0;
            break;
        }
    }else{// proactor
        if(users[sockfd].write()){
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            if(timer){
                adjust_timer(timer);
            }
        }else{
            deal_timer(timer,sockfd);
        }
    }
}

// 事件循环，循环监听epoll
void WebServer::eventLoop() {
    bool timeout = false;
    bool stop_server = false;

    while(!stop_server){
        int number = epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if(number < 0 && errno != EINTR){
            LOG_ERROR("%s","epoll failure");
            break;
        }
        // 处理有响应的fd
        for(int i = 0;i < number;i++){
            int sockfd = events[i].data.fd;

            // 处理新到的客户连接
            if(sockfd == m_listenfd){
                int sockfd = events[i].data.fd;
                bool flag = deal_client_data();
                if(flag == false){
                    continue;
                }else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                    // 服务端关闭连接，移除对应的定时器
                    util_timer *timer = users_timer[sockfd].timer;// 获取对应定时器
                    deal_timer(timer,sockfd);
                }else if((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)){
                    // 处理信号
                    bool flag = deal_with_signal(timeout,stop_server);
                    if(flag == false){
                        LOG_ERROR("%s","dealclientdata failure");
                    }
                }else if(events[i].events & EPOLLIN){
                    // 处理客户连接上接收到的数据
                    deal_with_read(sockfd);
                }else if(events[i].events & EPOLLOUT){
                    // 往客户端写数据
                    deal_with_write(sockfd);
                }
            }
        }
        if(timeout){
            utils.timer_handler();
            LOG_INFO("%s","timer tick");
            timeout = false;
        }
    }
}


