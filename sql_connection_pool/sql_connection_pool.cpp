#include "sql_connection_pool.h"

connection_pool::connection_pool(){
    m_cur_conn = 0;// 当前连接数0
    m_free_conn = 0;// 当前空闲的连接数
}

connection_pool::~connection_pool(){
    DestoryPool();
}

// 静态函数定义时不用加static关键字
connection_pool *connection_pool::GetInstance(){
    static connection_pool connPool;// C++11以后局部静态变量线程按安全
    return &connPool;
}

void connection_pool::init(std::string url,
              std::string user,
              std::string passwd,
              std::string databaseName,
              int port,
              int maxcount,
              int close_log){
    m_user = user;
    m_url = url;
    m_password = passwd;
    m_databasesName = databaseName;
    m_close_log = close_log;

    // 不直接将有关连接个数的值附上去，而是创建一个弄一个
    for(int i = 0; i < maxcount;i++){
        MYSQL *con = NULL;
        con = mysql_init(con);

        if(con == NULL){// 错误
            LOG_ERROR("MySQL ERROR");
            exit(1);
        }
        // 连接操作
        con = mysql_real_connect(con,url.c_str(),
                        user.c_str(),
                        passwd.c_str(),
                        databaseName.c_str(),
                        port,NULL,0);
        if(con == NULL){// 失败
            LOG_ERROR("MySQL ERROR");
            exit(1);
        }
        m_conn_list.push_back(con);// 将数据库描述符加入到数据库池中
        m_free_conn++;// 空闲连接++
    }
    reserve = sem(m_free_conn);// 使用可用的连接数初始化信号量
    m_max_conn = m_free_conn;// 实际创建出来的数据库连接数为最大的，以实际创建出来的为准。
    // 上面无序判断传进来的maxcount是否合法
}

MYSQL* connection_pool::GetConnection(){
    // 当有请求时，从数据连接池中获取一个可用连接，更新使用数量，和空闲链接
    MYSQL* con = NULL;
    if(m_conn_list.size() == 0){
        return NULL;
    }
    reserve.wait();
    m_mutex.lock();
    con = m_conn_list.front(); // 获取第一个
    m_conn_list.pop_front();// 去掉
    m_free_conn--;
    m_cur_conn++; // 当前已使用的链接++
    m_mutex.unlock();
    return con;
}

/*
信号量负责协调可等待的线程，互斥锁来监控线程的操作，限制某一时刻只有一个线程可以对临界区中的变量进行操作。
*/

// 释放当前使用的链接
bool connection_pool::ReleaseConnection(MYSQL * conn){
    /*
    在使用的时候根据GetConnection获取的连接，传进来销毁。
    想了一下，如果不是连接池中的连接，就不能删除，应该不用判断。
    */
    if(conn == NULL){ // 不是空的释放
        return false;
    }
    m_mutex.lock();
    m_conn_list.push_back(conn);
    m_free_conn++;
    m_cur_conn--;
    m_mutex.unlock();
    reserve.post();
    return true;
}

int connection_pool::GetFreeConn(){
    return m_free_conn;
}

void connection_pool::DestoryPool(){
    m_mutex.lock();
    if(m_conn_list.size() > 0){
        std::list<MYSQL*>::iterator it;
        for( it = m_conn_list.begin();it != m_conn_list.end();it++){
            MYSQL *con = *it;
            mysql_close(con);
        }
        m_cur_conn = 0;
        m_free_conn = 0;
        m_max_conn = 0;// 源代码中没有，我加上了，不影响。
        m_conn_list.clear();
    }
    m_mutex.unlock();
}

// 从connPool连接池中获取一个连接对象
connectionRAII::connectionRAII(MYSQL **con,connection_pool *connPool){
    *con = connPool->GetConnection();// 获取一个连接
    m_conRAII = *con;
    m_poolRAII = connPool;
}

connectionRAII::~connectionRAII(){
    m_poolRAII->ReleaseConnection(m_conRAII);// 析构的时候释放
}