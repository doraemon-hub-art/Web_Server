#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include "../common/common.h"
#include "../sql_connection_pool/sql_connection_pool.h"
#include "../lst_timer/lst_timer.h"
#include "../log/log.h"

class http_conn{

public:
    // 设置读取文件的名称m_read_file大小
    static const int FILE_NAME_LEN = 200;
    // 设置读缓冲区m_read_buf大小
    static const int READ_BUFFER_SIZE = 2048;
    // 设置写缓冲区m_write_buf大小
    static const int WRITE_BUFFER_SIZE = 1024;

    enum METHOD{// http方法
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };

    enum CHECK_STATE{// 主状态机状态
        CHECK_STATE_REQUESTLINE = 0,// 解析请求行
        CHECK_STATE_HEADER,// 解析请求头
        CHECK_STATE_CONTENT// 解析请求内容
    };

    enum HTTP_CODE{// 报文解析的结果
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

    enum LINE_STATUS{// 从状态机的状态
        LINE_OK = 0,    //  成功读取到一行
        LINE_BAD,   // 当前行数据出错
        LINE_OPEN   // 数据不完整
    };

public:

    http_conn(){}
    ~http_conn(){}

public:

    // 初始化
    void init(int socked,const sockaddr_in &addr,char*,
        int,int,std::string user,std::string passwd,
        std::string sqlname);

    // 关闭http连接
    void close_conn(bool real_close = true);

    // 
    void process();

    // 读取浏览器发来的全部数据
    bool read_once();

    // 响应报文写入
    bool write();

    sockaddr_in* get_address(){
        return &m_address;
    }
    
    // CGI使用线程池初始化数据库表
    void initmysql_result(connection_pool *connPool);
    int timer_flag;
    int improv;

private:
    void init();
    HTTP_CODE process_read(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char* text);// 解析请求行
    HTTP_CODE parse_headers(char* text);// 解析请求头
    HTTP_CODE parse_content(char* text);// 解析请求内容
    HTTP_CODE do_request();

    // 将指针向后偏移，指向未处理的字符。
    char* get_line(){
        return m_read_buf+m_start_line;
    }

    // 从状态机读取一行，分析是请求报文的哪一部分
    LINE_STATUS parse_line();
    void unmap();

    // 根据响应报文格式，生成对应8个部分，以下函数均由do_request调用
    bool add_response(const char* format,...);
    bool add_content(const char* content);
    bool add_status_line(int status,const char* title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;// epoll句柄
    static int m_user_count;// 用户数
    MYSQL* mysql;
    int m_state;// 读0，写1

private:
    int m_sockfd;
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];// 读缓冲区
    int m_read_idx;// 读到哪了
    int m_check_idx;// 检查到哪了
    int m_start_line; // 已经解析的字符
    int m_write_buf[WRITE_BUFFER_SIZE];// 写缓冲区
    int m_wirte_idx;// 写到哪了
    CHECK_STATE m_check_state;// 主状态机状态
    METHOD m_method;
    char m_real_file[FILE_NAME_LEN];// 读取文件名称
    
    // HTTP请求相关信息
    char* m_url;// 地址
    char* m_version;
    char* m_host;
    int m_content_length;
    bool m_linger;
    char* m_file_address;// 文件路径
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count; 
    int cgi;   // 是否启用的POST
    char* m_string; // 存储请求头数据
    int bytes_to_send;
    int bytes_have_send;
    char* doc_root;

    std::map<std::string,std::string>m_users;
    int m_TRIGMod;
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];

};

#endif