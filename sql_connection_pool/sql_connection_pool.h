#ifndef SQL_CONNECTION_H
#define SQL_CONNECTION_H

#include "../common/common.h"
#include "../log/log.h"
#include "../lock/locker.h"

// 数据库连接池
class connection_pool{

private:

    connection_pool();
    // RAII机制销毁连接池
    ~connection_pool();

public:
    // 获取数据库连接
    MYSQL* GetConnection(); 
    // 释放连接
    bool ReleaseConnection(MYSQL* conn); 
    // 获取当前空闲连接个数
    int GetFreeConn(); 
    // 销毁所有连接
    void DestoryPool(); 
    
    // 单例模式-获取当前实例
    static connection_pool* GetInstance();

    // 初始化-地址-用户-密码-数据名-端口-最大连接数-日志开关
    void init(std::string url,
              std::string user,
              std::string passwd,
              std::string databaseName,
              int port,
              int maxcount,
              int close_log);
private:

    int m_max_conn; // 最大连接数
    int m_cur_conn; // 当前已使用的连接数
    int m_free_conn; // 当前空闲的连接数
    locker m_mutex; // 互斥锁
    std::list<MYSQL*>m_conn_list;// 连接池
    sem reserve;   // 信号量，信号量实现多线程间的争夺同步机制。初始化为数据库连接总数。

public:
    std::string m_url; // 主机地址
    std::string m_port; // 数据库端口
    std::string m_user; // 登录数据库用户
    std::string m_password; // 登录数据库密码
    std::string m_databasesName; // 数据库名称
    int m_close_log; // 日志开关
};

// RAII机制释放数据库连接
// 将数据库连接的获取与释放通过RAII机制封装，避免手动释放
// 即，使用的时候用connectionRAII创建一个对象即可，
// 问题是，到底该怎么用呢，这也没有给外部的接口啊。
// 通过传进去一个二级的MYSQL指针进行使用。
class connectionRAII{

public:
    // 通过二级指针修改对应的内容，这里还没有体现，还得往后看
    connectionRAII(MYSQL **con,connection_pool* connPoll);
    ~connectionRAII();

private:
    MYSQL *m_conRAII;
    connection_pool* m_poolRAII;
};



#endif