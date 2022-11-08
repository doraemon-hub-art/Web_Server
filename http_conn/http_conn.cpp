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

    strcpy(sql_user,user.c_str());
    strcpy(sql_passwd,passwd.c_str());
    strcpy(sql_name,sqlname.c_str());

    init();
}

// 初始化
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

// 从状态机，用于分析出一行内容
// 返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line() {
    char temp;
    for(;m_check_idx < m_read_idx;m_check_idx++){
        temp = m_read_buf[m_check_idx];
        if(temp == '\r'){
            if(m_check_idx + 1 == m_read_idx){
                return LINE_OPEN;// 没读完-行内容不全
            }else if(m_read_buf[m_check_idx+1] == '\n'){
                m_read_buf[m_check_idx++] = '\0';
                m_read_buf[m_check_idx++] = '\0';
                return LINE_OPEN;// 成功读取到一行
            }
            return LINE_BAD;// 行出错
        }else if(temp == '\n'){
            // 一开始看这个感觉很奇怪，不知道这行是干嘛的
            // 作用: 因为可能不是一次性读(发)过来的，比如前面读到\r,这里到\n，所以要如此判断。
            if(m_check_idx > 1 && m_read_buf[m_check_idx-1] == '\r'){
                m_read_buf[m_check_idx-1] = '\0';
                m_read_buf[m_check_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;// 没读完
}

// 循环读取数据，直到无数据可读或对方关闭连接
// 非阻塞ET工作模式下，需要一次性将数据读取完成。因为只提醒一次。
bool http_conn::read_once() {
    if(m_read_idx >= READ_BUFFER_SIZE){
        return false;
    }
    int bytes_read = 0;// 读取数量

    // LT读取数据-条件模式读取数据
    if(m_TRIGMod == 0){
        // 边界控制读取多少
        bytes_read = recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
        m_read_idx += bytes_read;

        if(bytes_read <= 0){
            return false;
        }
        return true;
    }else{// ET-模式读取-要一次读光
        while (true){
            bytes_read = recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
            if(bytes_read == -1){// ET模式要将描述符设为非阻塞
                if(errno == EAGAIN ||  errno == EWOULDBLOCK){// 读光
                    break;
                }
                return false;
            }else if(bytes_read == 0){// 链接关闭
                return false;
            }
        }
        m_read_idx += bytes_read;
    }
    return true;
}

// 解析http请求行，获取请求方法，目标url即http版本号
// 请求行格式: GET /index.html HTTP/1.1
http_conn::HTTP_CODE http_conn::parse_request_line(char *text) {
    m_url = strpbrk(text," \t");
    if(!m_url){
        return BAD_REQUEST;// 错误请求
    }
    *m_url++ = '\0';
    char* method = text;

    // 判断请求方式，我们只处理GET和POST方法
    if(strcasecmp(method,"GET") == 0){
        m_method = GET;
    }else if(strcasecmp(method,"POST") == 0){
        m_method = POST;
        cgi = 1;
    }else{
        return BAD_REQUEST;
    }

    m_url += strspn(m_url," \t");
    m_version = strpbrk(m_url," \t");

    if(!m_version){
        return BAD_REQUEST;
    }
    *m_version++ ='\0';
    m_version += strspn(m_version," \t");
    if(strcasecmp(m_version,"HTTP/1.1") != 0){
        return BAD_REQUEST;
    }

    // 分析请求路径
    if(strncasecmp(m_url,"http://",7) == 0){
        m_url += 7;
        m_url = strchr(m_url,'/');
    }
    if(strncasecmp(m_url,"https://",8) == 0){
        m_url += 8;
        m_url = strchr(m_url,'/');
    }

    if(!m_url || m_url[0] != '/'){// 错误请求
        return BAD_REQUEST;
    }

    // 当url为/时，判断显示界面
    if(strlen(m_url) == 1){// 只为/,说明访问首页
        strcat(m_url,"judge.html");// 将src追加到dest尾部
    }
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;// 请求不完整
}

// 解析http请求头，的一个信息，XX:XXX,这是一个信息，即一行。
http_conn::HTTP_CODE http_conn::parse_headers(char *text) {
    if(text[0] == '\0'){
        if(m_content_length != 0){
            m_check_state = CHECK_STATE_CONTENT;// 请求不完整
            return NO_REQUEST;
        }
        return GET_REQUEST;// 获得了一个完整请求
    }else if(strncasecmp(text,"Connection:",11) == 0){
        text += 11;
        text += strspn(text," \t");// \t之后就是内容,即 text: xx:strspn，即指向s了。
        if(strcasecmp(text,"keep-alive") == 0){
            m_linger = true;
        }
    }else if(strncasecmp(text,"Content-length:",15) == 0){
        text += 15;
        text += strspn(text," \t");
        m_content_length = atol(text);
    }else if(strncasecmp(text,"Host:",5) == 0){
        text += 5;
        text += strspn(text," \t");
        m_host = text;
    }else{// 未知请求头
        LOG_INFO("oop! unknow header: %s",text);
        return BAD_REQUEST;
    }
}

// 判断http请求是否被完整读入
http_conn::HTTP_CODE http_conn::parse_content(char *text) {
    // 当前读取到的位置，大于等于，内容的长度和已经检查的长度
    if(m_read_idx >= m_content_length + m_check_idx){
        text[m_content_length] = '\0';
        // POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;// 获得了一个完整的请求行
    }
    return NO_REQUEST;// 不完整
}


// 分析http请求的入口函数——主状态机
http_conn::HTTP_CODE http_conn::process_read() {
    LINE_STATUS line_status = LINE_OK;// 从状态机状态
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;

    while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) ||
    ((line_status = parse_line()) ==LINE_OK)){
        text = get_line();// 指向未处理的字符
        m_start_line = m_check_idx;
        LOG_INFO("%s",text);
        switch (m_check_state) {// 主状态机状态变化

        case CHECK_STATE_REQUESTLINE:{
            ret = parse_request_line(text);
            if(ret == BAD_REQUEST){
                return  BAD_REQUEST;
            }
            break;
        }
        case CHECK_STATE_HEADER:{
            ret = parse_headers(text);
            if(ret == BAD_REQUEST){
                return  BAD_REQUEST;
            }else if(ret == GET_REQUEST){
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:{
            ret = parse_content(text);
            if(ret == GET_REQUEST){
                 return do_request();
            }
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

// 做响应
http_conn::HTTP_CODE http_conn::do_request() {

}
