#include "/webserver/webserver.h"

int main(int argc,char* argv[]){

    // 数据库信息，账号-密码-数据库
    std::string user = "root";
    std::string passwd = "123";
    std::string datebasesname = "yourdb";
;
    int port = 9006;// 端口
    int log_write = 0;// 日志写入方式，0同步
    int trig_mode = 0;// 触发组合模式，listen Lt + connfd Lt
    int listen_trig_mode = 0;// listen 触发模式 Lt
    int conn_trig_mode = 0; // conn 触发模式 Lt
    int opt_linger = 0;// 优雅关闭链接，不使用
    int sql_num = 8;// 数据库连接池
    int thread_num = 8; // 线程池内线程数量
    int close_log = 0;// 不关闭日志
    int actor_model = 0;// 魔法模型选择，默认是proactor

    WebServer server;

    // 初始化
    server.init(port,
            user,
            passwd,
            datebasesname,
            log_write,
            opt_linger,
            trig_mode,
            sql_num,
            thread_num,
            close_log,
            actor_model);

    // 日志
    server.log_write();

    // 初始化-数据库连接池
    server.sql_pool();

    // 创建-线程池
    server.thread_pool();

    // 设置-触发模式
    server.trig_mode();

    // 监听
    server.eventListen();

    // 运行
    server.eventLoop();

    return 0;
}