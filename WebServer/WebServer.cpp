//
// Created by diode on 23-6-28.
//

#include <sys/socket.h>
#include <cassert>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <cstring>
#include <iostream>
#include "WebServer.h"
#include "../Config/Config.h"
#include "../Utils/Utils.h"

int WebServer::connection_num_ = 0;

WebServer::WebServer() {
    server_fd_ = -1;
    elegant_close_send_ = Config::GetSingleton_()->elegant_close_send_;
    reuse_ = Config::GetSingleton_()->reuse_;
    server_port_ = Config::GetSingleton_()->server_port_;
    listen_queue_max_num_ = Config::GetSingleton_()->listen_queue_max_num_;
    epoll_listen_max_num_ = Config::GetSingleton_()->epoll_listen_max_num_;
    epoll_fd_ = -1;
    listen_level_trigger_mode_ = Config::GetSingleton_()->listen_level_trigger_mode_;
    connection_level_trigger_mode_ = Config::GetSingleton_()->connection_level_trigger_mode_;
    one_shot_ = Config::GetSingleton_()->one_shot_;
    events_max_size_ = Config::GetSingleton_()->events_max_size_;
    events_ = new struct epoll_event[events_max_size_];
    epoll_wait_max_num_ = Config::GetSingleton_()->epoll_wait_max_num_;
    connection_max_num_ = Config::GetSingleton_()->connection_max_num_;
    connections_ = new HTTPConnection[connection_max_num_];
    reactor_ = Config::GetSingleton_()->reactor_;
}

WebServer::~WebServer() {
    delete[] events_;
    delete[] connections_;
}

void WebServer::EventListen() {
    server_fd_ = socket(PF_INET, SOCK_STREAM, 0);
    assert(server_fd_ > 0);
    if (elegant_close_send_) {
        struct linger tmp = {1, 1};
        setsockopt(server_fd_, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    if (reuse_) {
        int tmp = 1;
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(tmp));
    }
    int ret = 0;
    struct sockaddr_in server_address;
    server_address.sin_family = PF_INET;
    server_address.sin_port = htons(server_port_);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    ret = bind(server_fd_, (sockaddr *) &server_address, (socklen_t) sizeof(server_address));
    assert(ret >= 0);
    ret = listen(server_fd_, listen_queue_max_num_);
    assert(ret >= 0);

    epoll_fd_ = epoll_create(epoll_listen_max_num_);
    assert(epoll_fd_ != -1);
    Utils::AddEpollFd(server_fd_, epoll_fd_, true, listen_level_trigger_mode_, false);
}

void WebServer::EpollLoop() {
    bool exists = false;
    while (!exists) {
        int count = epoll_wait(epoll_fd_, events_, epoll_wait_max_num_, -1);
        for (int i = 0; i < count; ++i) {
            int client_fd = events_[i].data.fd;
            if (server_fd_ == client_fd) {  // 建立连接的事件。
                DealConnection();
            } else if (events_[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                Utils::DelEpollFd(client_fd, epoll_fd_);
            } else if (events_[i].events & EPOLLIN) {
                DealRead(client_fd);
            } else if (events_[i].events & EPOLLOUT) {
                DealWrite(client_fd);
            }
        }
    }
}

void WebServer::DealConnection() {
    struct sockaddr_in client_address;
    socklen_t client_address_size = sizeof(client_address);
    if (listen_level_trigger_mode_) {  // 水平触发
        int client_fd = accept(server_fd_, (sockaddr *) &client_address, (socklen_t *) &client_address_size);
        if (client_fd < 0) {
            return;
        }
        if (connection_num_ >= connection_max_num_) {
            return;
        }
        connections_[client_fd].EstablishConnection(client_fd, epoll_fd_, client_address);
        Utils::AddEpollFd(client_fd, epoll_fd_, true, connection_level_trigger_mode_, one_shot_);
    } else {
        while (true) {
            int client_fd = accept(server_fd_, (sockaddr *) &client_address, (socklen_t *) &client_address_size);
            if (client_fd < 0) {
                return;
            }
            if (connection_num_ >= connection_max_num_) {
                return;
            }
            connections_[client_fd].EstablishConnection(client_fd, epoll_fd_, client_address);
            Utils::AddEpollFd(client_fd, epoll_fd_, true, connection_level_trigger_mode_, one_shot_);
        }
    }
}

void WebServer::DealRead(int client_fd) {
    if (reactor_) {  // 读写以及处理全部交给子线程
        ThreadPool::GetSingleton()->append(connections_ + client_fd, HTTPConnection::WORK_MODE::READ);  // why
    } else {
        if (connections_[client_fd].ReceiveMessage()) {
            ThreadPool::GetSingleton()->append(connections_ + client_fd, HTTPConnection::WORK_MODE::PROCESS);
        } else {
            Utils::DelEpollFd(client_fd, epoll_fd_);
        }
    }
}

void WebServer::DealWrite(int client_fd) {
    if (reactor_) {
        ThreadPool::GetSingleton()->append(connections_ + client_fd, HTTPConnection::WORK_MODE::WRITE);
    } else {
        std::cout << "发送操作" << std::endl;
        std::string line = "HTTP/1.1 200 OK\r\n" "Content-Type: text/html; charset=utf-8\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, line.c_str(), line.size(), 0);
        connections_[client_fd].Init();
        Utils::ModEpollFd(client_fd, epoll_fd_, true, Config::GetSingleton_()->connection_level_trigger_mode_,
                          Config::GetSingleton_()->one_shot_);
    }
}