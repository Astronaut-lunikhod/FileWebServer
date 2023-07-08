//
// Created by diode on 23-7-6.
// 定时器的类。

#ifndef FILEWEBSERVER_TIMER_H
#define FILEWEBSERVER_TIMER_H

#include <time.h>
#include <unordered_set>
#include <netinet/in.h>
#include "../Utils/Utils.h"
#include "../WebServer/WebServer.h"

struct connection {  // 定时器中的内容。
    std::string session_;  // 这一批连接的session。
    sockaddr_in client_address;
    int socket_fd_;  // 该连接使用的文件描述符。
};

class Timer {  // 一个定时器，最终结果是构建一个升序链表。
public:
    Timer() : pre(nullptr), next(nullptr) {};
    time_t expire_;  // 当前定时器记录的超市时间。
    connection con_;

    Timer *pre;
    Timer *next;
};

class sort_timer_list {  // 升序链表。
private:
    Timer *head;  // 头尾节点不会为空，如果有内容的话。
    Timer *tail;
    pthread_mutex_t  mutex_;
public:
    sort_timer_list();

    ~sort_timer_list();

    void AddTimer(Timer *timer);

    void InsertTime(Timer *timer, Timer *mid);  // 因为不能直接判断插在那里，所以需要从mid开始搜索，往后看，可以插入哪里。

    void AdjustTimer(Timer *timer);

    void DelTimer(Timer *timer);

    void Tick(int epoll_fd);
};

#endif