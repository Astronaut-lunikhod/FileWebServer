//
//  Created by diode on 23-6-28.
//  什么max_num, 模式，统统由配置类限定。
//

#ifndef FILEWEBSERVER_CONFIG_H
#define FILEWEBSERVER_CONFIG_H

#include <pthread.h>

class Config {
public:
    static Config *GetSingleton_();
private:
    static Config *config_singleton_;
    static pthread_mutex_t singleton_mutex_;

    ~Config() {

    }

public:
    int thread_num_;  // 线程池的配置
    int max_request_queue_size_;
    bool reactor_;

    bool elegant_close_send_;  // webserver配置
    bool reuse_;
    int server_port_;  // server_fd_监听的端口号。
    int listen_queue_max_num_;  // 在全连接队列中同时建立连接的最大数量。
    int epoll_listen_max_num_;  // epoll同时监听的最大数量。
    bool listen_level_trigger_mode_;  // 建立连接的请求使用水平触发
    bool connection_level_trigger_mode_;  // 已经建立的连接使用水平触发
    bool one_shot_;
    int events_max_size_;
    int epoll_wait_max_num_;  //epoll_wait返回的最大数量。
    int connection_max_num_;  // 最大的连接数。

    int read_buffer_max_len;  // HTTP

private:
    Config() {
        thread_num_ = 1;
        max_request_queue_size_ = 10;
        reactor_ = true;
        elegant_close_send_ = true;
        reuse_ = true;
        server_port_ = 7777;
        listen_queue_max_num_ = 1000;
        epoll_listen_max_num_ = 1000;
        listen_level_trigger_mode_ = true;
        connection_level_trigger_mode_ = true;
        one_shot_ = true;
        events_max_size_ = 1000;
        epoll_wait_max_num_ = 100;
        connection_max_num_ = 1000;
        read_buffer_max_len = 1024;
    }
};

#endif
