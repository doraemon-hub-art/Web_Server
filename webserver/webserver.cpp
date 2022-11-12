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

