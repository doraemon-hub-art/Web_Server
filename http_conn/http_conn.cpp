#include "http_conn.h"

// 定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

locker m_lock;
std::map<std::string,std::string>users;

// 初始化用户数
int http_conn::m_user_count = 0;

int http_conn::m_epollfd = -1;

// 初始化数据库连接
void http_conn::initmysql_result(connection_pool *connPool) {
    // 先从线程池中取一个连接
    MYSQL* mysql = nullptr;
    connectionRAII mysqlCon(&mysql,connPool);

    // 在user表中检索username,passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user")){
        LOG_ERROR("SELECT    error:%s\n", mysql_error(mysql));
    }

    // 从表中检索完整的结果集
    MYSQL_RES* result = mysql_store_result(mysql);

    // 返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    // 返回所有字段结构的数组
    MYSQL_FIELD* field = mysql_fetch_fields(result);

    // 从结构及中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result)){
        std::string temp1(row[0]);
        std::string temp2(row[1]);
        users[temp1] = temp2;
    }
}

// 设置文件描述符为非阻塞
int setnonblocking(int fd){
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option|O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

// 将内核事件表注册——读事件
// ET模式，选择开启EPOLLONESHOT-防止多线程引起竞态
void addfd(int epollfd,int fd,bool one_shot,int TRIGMode){
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
}

// 从内核事件表删除描述符
void removefd(int epollfd,int fd){
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

// 将事件重置为EPOLLONESHOT
void  modfd(int epollfd,int fd,int ev,int TRIGMode){
    epoll_event event;
    event.data.fd = fd;

    if(TRIGMode == 1){
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    }else{
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    }
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

// 关闭一个连接，关闭一个数-1
void http_conn::close_conn(bool real_close) {
    if(real_close && m_sockfd != -1){
        std::cout<<"close:"<<m_sockfd<<std::endl;
        removefd(m_epollfd,m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

// 初始化连接，外部调用初始化套接字地址。
// 函数声明参数名可以省略，定义时如果不使用也可以不写，但是不建议
void http_conn::init(int socked, const sockaddr_in &addr, char *root, int TRIGMode, int close_log, std::string user, std::string passwd,
                     std::string sqlname) {
    m_sockfd = socked;
    m_address = addr;

    addfd(m_epollfd,socked, true,m_TRIGMod);
    m_user_count++;// 用户总数++

    // 当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
    doc_root = root;
    m_TRIGMod = TRIGMode;
    m_close_log = close_log;

    strcpy();

}

void http_conn::init() {
    mysql = nullptr;
    bytes_have_send = 0;
    bytes_to_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;// 初始化主状态机状态为分析请求行
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_check_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf,'\0',READ_BUFFER_SIZE);
    memset(m_write_buf,'\0',WRITE_BUFFER_SIZE);
    memset(m_real_file,'\0',FILE_NAME_LEN);
}


// 初始化新接收的连接
// check_state默认为分析请求行状态
