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
}