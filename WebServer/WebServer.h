//
// Created by diode on 23-6-29.
// 主线程调用的处理逻辑类。
//

#ifndef FILEWEBSERVER_WEBSERVER_H
#define FILEWEBSERVER_WEBSERVER_H

#include <unistd.h>
#include <sys/socket.h>
#include <cassert>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <cerrno>
#include <hiredis/hiredis.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <unordered_set>
#include "../Config/Config.h"
#include "../Utils/Utils.h"
#include "../HTTPConnection/HTTPConnection.h"
#include "../ThreadPool/ThreadPool.h"
#include "../WebServer/WebServer.h"
#include "../Redis/Redis.h"
#include "../timer/timer.h"

class HTTPConnection;
class sort_timer_list;
class Timer;

class WebServer {
private:
    int server_fd_;  // 服务器监听的文件描述符。
    int epoll_fd_;  // epoll的文件描述符。
    bool elegant_close_;  // 是否优雅关闭。
    bool reuse_;  // 是否复用地址。
    unsigned server_port_;  // 使用的端口号。
    unsigned listen_queue_max_size_;  // listen的队列中，可以储存的全连接的最大数量。
    unsigned epoll_meanwhile_listen_max_num;  // epoll同时监测的最大数量。
    bool listen_level_trigger_;  // 监听建立连接的部分是否使用水平触发。
    bool connection_level_trigger_;  // 已经建立的连接是否使用水平触发。
    bool listen_one_shot_;  // 同理，监听和已建立的是否使用one_shot。
    bool connection_one_shot_;
    unsigned epoll_events_max_size_;  // 容器的大小。
    struct epoll_event *events_;  // 存放epoll监测结果的容器。
    unsigned connection_max_num_;  // 同时连接的最大连接数。
    bool reactor_;  // 是否使用reactor事件模型。
    int pipe_[2];  // 读端和写端。
public:
    static unsigned connection_num_;  // 当前建立连接的数量。
    static HTTPConnection *connections_;  // 记录连接。
    static pthread_mutex_t session_map_mutex_;  // 操作session_map需要锁。
    static std::unordered_map<std::string, std::unordered_set<int>> session_map_;  // 已经建立连接的对象以及对应的session建立的map。
    static sort_timer_list sort_timer_list_;  // 计时器的升序链表。
    static Timer **timers_;  // 对应fd个数的计时器，方便直接获取，用于操作链表。
private:
    void EstablishConnection();  // 建立连接。

    void DealRead(int client_fd);  // 处理读信号。

    void DealWrite(int client_fd);  // 处理写信号。

    void DealSig(int client_fd, bool &exit, bool &timeout);  // 处理alarm信号。
public:
    WebServer();

    ~WebServer();

    void InitialConnection();  // 开始监听目标端口，并开启epoll进行IO多路复用。

    void EpollLoop();  // 重复对Epoll的结果进行处理。
};

#endif