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

// 解析http请求行，获取请求方法，目标url及http版本号
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
    // 将初始化的m_real_file赋值为网站根目录
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);

    // 找到m_url中/ 的位置
    const char *p = strchr(m_url, '/');

    // 处理cgi
    // 实现登录和注册校验
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3')) {

        // 根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        char *m_url_real = (char *) malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcpy(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILE_NAME_LEN - len - 1);
        free(m_url_real);

        // 将用户名和密码提取出来
        // user=123&passwd=123
        char name[100], password[100];
        int i = 0;
        for (i = 5; m_string[i] != '&'; i++) {
            name[i - 5] = m_string[i];
        }
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; i++, j++) {
            password[j] = m_string[i];
        }
        password[j] = '\0';

        if (*(p + 1) == 3) {// 注册
            // 如果是注册，先检测数据库中是否有重名的
            // 没有重名的，进行增加数据
            char *sql_insert = (char *) malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username,passwd) VALUES(");
            // 追加拼接字符串
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");
            if (users.find(name) == users.end()) {// 没找到
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);// 执行语句
                users.insert(std::pair<std::string, std::string>(name, password));
                m_lock.unlock();

                if (!res) {
                    strcpy(m_url, "/log.html");
                } else {
                    strcpy(m_url, "/registerError.html");
                }

            } else if (*(p + 1) == '2') {// 登录
                // 若输入的用户名和密码可以查询到
                if (users.find(name) != users.end() && users[name] == password) {
                    strcpy(m_url, "/welcome.html");
                } else {
                    strcpy(m_url, "logError.html");
                }
            }
        }

        // 如果请求资源为/0，表示跳转注册界面
        if (*(p + 1) == '0') {
            char* m_url_real = (char*) malloc(sizeof(char)*200);
            strcpy(m_url_real,"/register.html");

            // 将网站目录和/register.html进行拼接，更新到m_real_file中
            strncpy(m_real_file+len,m_url_real, strlen(m_url_real));

            free(m_url_real);
        }else if(*(p+1) == '1'){// 如果请求资源为/1，则表示跳转登录界面
            char *m_url_real = (char*)malloc(sizeof(char)*200);
            strcpy(m_url_real,"/log.html");

            // 将网站目录和/log.html进行拼接，更新到m_real_file中
            strncpy(m_real_file+len,m_url_real, strlen(m_url_real));

            free(m_url_real);
        }else if(*(p+1) == '5'){// 请求资源为/5，请求图片资源
            char* m_url_real = (char*)malloc(sizeof(char)*200);
            strcpy(m_url_real,"/picture.html");
            strncpy(m_real_file+len,m_url_real,strlen(m_url_real));

            free(m_url_real);
        }else if(*(p+1) == '6'){// 请求资源为/6，请求视频资源
            char* m_url_real = (char*)malloc(sizeof(char)*200);
            strcpy(m_url_real,"video.html");
            strncpy(m_real_file+len,m_url_real, strlen(m_url_real));

            free(m_url_real);
        }else if(*(p+1) == '7'){// 请求资源为/7,请求一个小彩蛋
            char* m_url_real = (char*)malloc(sizeof(char)*200);
            strcpy(m_url_real,"fans.html");
            strncpy(m_real_file+len,m_url_real, strlen(m_url_real));

            free(m_url_real);
        }else{// 均不符合以上请求资源的类型
            //如果以上均不符合，即不是登录和注册，直接将url与网站目录拼接
            //这里的情况是welcome界面，请求服务器上的一个图片
            strncpy(m_real_file+len,m_url,FILE_NAME_LEN-len-1);
        }

        // 通过stat获取请求资源文件的信息，成功则将信息更新到m_file_stat结构体
        // 失败返回NO_RESOURCE状态，表示资源不存在
        if(stat(m_real_file,&m_file_stat) < 0){
            return NO_RESOURCE;
        }

        // 判断文件的权限，是否可读
        if(!(m_file_stat.st_mode & S_IROTH)){
            return FORBIDDEN_REQUEST;
        }

        // 判断文件类型，如果是目录，则返回BAD_REQUEST,表示请求报文有误
        if(S_ISDIR(m_file_stat.st_mode)){
            return BAD_REQUEST;
        }

        // 以只读方式获取文件描述符，通过mmap将该文件映射到内存中。
        int fd = open(m_real_file,O_RDONLY);
        m_file_address = (char*)mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0);

        // 清理资源——避免文件描述符浪费和占用 close掉
        close(fd);

        // 表示请求文件存在，且可以访问到。
        return FILE_REQUEST;
    }
}

void http_conn::unmap() {
    if(m_file_address){
        munmap(m_file_address,m_file_stat.st_size);
        m_file_address = 0;
    }
}

// 响应-主线程调用-返回给用户(浏览器)
bool http_conn::write() {
    int temp = 0;

    // 要发送的数据长度为0
    // 表示响应报文为空，但是一般不会出现这种情况
    if(bytes_to_send == 0){
        modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMod);
        init();
        return true;
    }

    while(1){
        // 将响应报文的状态行，消息头，空行，响应正文，发送给浏览器端
        temp = writev(m_sockfd,m_iv,m_iv_count);

        // 写失败
        if(temp < 0){
            if(errno == EAGAIN){// 判断缓冲区是不是已经满了
                // 重新注册写事件
                modfd(m_epollfd,m_sockfd,EPOLLOUT,m_TRIGMod);
                return true;
            }
            unmap();// 解除映射
            return false;
        }

        // 正常发送
        bytes_have_send += temp;// 更新已发送的字节
        bytes_to_send -= temp;// 更新要发送的数据大小

        if(bytes_have_send >= m_iv[0].iov_len){// 超过发送数据缓冲区的长度
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }else{
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len-bytes_have_send;
        }

        if(bytes_have_send <= 0){
            unmap();
            modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMod);

            if(m_linger){
                init();
                return true;
            }else{
                return false;
            }

        }
    }
}

// 下面几个add_xx函数调用此函数，来更新m_write_idx指针和缓冲区m_write_buf中的内容
bool http_conn::add_response(const char *format, ...) {
    // 如果写入的内容超出m_write_buf大小则报错
    if(m_write_idx  >= WRITE_BUFFER_SIZE){
        return false;
    }

    // 定义可变参数列表
    va_list arg_list;

    // 将变量arg_list初始化为转入参数
    va_start(arg_list,format);

    // 将数据format从可变参数列表写入缓冲区写，返回写入数据的长度。
    // int len = vsnprintf(m_write_buf+m_write_idx,WRITE_BUFFER_SIZE-1-m_write_idx,format,arg_list);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx,format,arg_list);

    // 如果写入的数据长度超过缓冲区剩余空间，则报错
    if(len > WRITE_BUFFER_SIZE - 1 - m_write_idx){
        va_end(arg_list);
        return false;
    }

    // 更新m_write_idx位置
    m_write_idx += len;
    // 清空可变参数列表
    va_end(arg_list);

    LOG_INFO("request:%s",m_write_buf);
    return true;
}

// 添加状态行
bool http_conn::add_status_line(int status, const char *title) {
    return add_response("%s %d %s\r\n","HTTP/1.1",status,title);
}

// 添加消息报头，具体的添加文本长度，连接状态和空行
bool http_conn::add_headers(int content_length) {
    return add_content_length(content_length) && add_linger() && add_blank_line();
}

// 添加Content-Length，表示响应报文的长度
bool http_conn::add_content_length(int content_length) {
    return add_response("Content-Length:%d\r\n",content_length);
}

//添加文本类型，这里是html
bool http_conn::add_content_type() {
    return add_response("Content-Type:%s\r\n","text/html");
}

// 添加连接状态，通知浏览器端是保持连接还是关闭
bool http_conn::add_linger() {
    return add_response("Connection:%s\r\n",(m_linger==true)?"keep-alive":"close");
}

// 添加空行
bool http_conn::add_blank_line() {
    return add_response("s","\r\n");
}

// 添加文本content
bool http_conn::add_content(const char *content) {
    return add_response("%s",content);
}

// 根据do_request的返回状态，子线程调用向m_write_buf中写入响应报文
bool http_conn::process_write(HTTP_CODE ret) {
    switch (ret) {// 内部错误响应500
    case INTERNAL_ERROR:{
        // 状态行
        add_status_line(500,error_500_title);
        // 消息报头
        add_headers(strlen(error_500_form));
        if(!add_content(error_500_form)){
            return false;
        }
        break;
    }
    // 报文语法有错误，404
    case BAD_REQUEST:{
        add_status_line(404,error_404_title);
        add_headers(strlen(error_404_form));
        if(!add_content(error_404_form)){
            return false;
        }
        break;
    }
    // 资源没有访问权限，403
    case FORBIDDEN_REQUEST:{
        add_status_line(403,error_403_title);
        add_headers(strlen(error_403_form));
        if(!add_content(error_403_form)){
            return false;
        }
        break;
    }
    // 文件存在，200
    case FILE_REQUEST:{
        add_status_line(200,ok_200_title);
        // 如果请求的资源存在
        if(m_file_stat.st_size != 0){
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;

            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;

            //发送的全部数据为响应报文头部信息和文件大小
            bytes_have_send = m_write_idx + m_file_stat.st_size;
            return  true;
        }else{// 请求的资源不存在
            const char* ok_string="<html><body></body></html>";// 返回空白html文件
            add_headers(strlen(ok_string));
            if(!add_content(ok_string)){
                return false;
            }

        }
    }
    default:
        return false;
    }

    // 出FILE_REQUEST状态外，其余状态只申请一个iovec,指向响应报文缓冲区
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}

void http_conn::process() {
    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST){
        modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMod);
        return;
    }
    bool write_ret = process_write(read_ret);
    if(!write_ret){
        close_conn();
    }
    modfd(m_epollfd,m_sockfd,EPOLLOUT,m_TRIGMod);
}
