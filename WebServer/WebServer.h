//
// Created by diode on 23-6-28.
//

#ifndef FILEWEBSERVER_WEBSERVER_H
#define FILEWEBSERVER_WEBSERVER_H

#include "../HTTPConnection/HTTPConnection.h"
#include "../ThreadPool/ThreadPool.h"

class HTTPConnection;

class WebServer {
private:
    int server_fd_;
    bool elegant_close_send_;
    bool reuse_;
    int server_port_;
    int listen_queue_max_num_;
    int epoll_listen_max_num_;
    int epoll_fd_;
    bool listen_level_trigger_mode_;
    bool connection_level_trigger_mode_;
    bool one_shot_;
    int events_max_size_;
    struct epoll_event *events_;
    int epoll_wait_max_num_;
    int connection_max_num_;
    HTTPConnection *connections_;
    bool reactor_;
public:
    static int connection_num_;
    WebServer();

    ~WebServer();

    void EventListen();  // 开始监听

    void EpollLoop();  // 开始循环

    void DealConnection();  // 建立连接

    void DealRead(int client_fd);  // 处理读取事件

    void DealWrite(int client_fd);  // 处理写事件
};

#endif
