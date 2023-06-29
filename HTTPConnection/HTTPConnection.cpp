//
// Created by diode on 23-6-29.
//

#include <cstring>
#include <cerrno>
#include <iostream>
#include "HTTPConnection.h"


void
HTTPConnection::EstablishConnection(int client_fd, int epoll_fd, const sockaddr_in &client_address) {  // 建立连接，初始化对象。
    client_fd_ = client_fd;
    epoll_fd_ = epoll_fd;
    client_address_ = client_address;
    ++WebServer::connection_num_;
    read_buffer_max_len_ = Config::GetSingleton_()->read_buffer_max_len;
    read_buffer_ = new char[read_buffer_max_len_];
    memset(read_buffer_, '\0', read_buffer_max_len_);
    read_next_idx_ = 0;
    main_next_idx_ = 0;
    sub_next_idx_ = 0;
}

void HTTPConnection::Init() {  // 长连接,重新刷新状态。
    memset(read_buffer_, '\0', read_buffer_max_len_);
    read_next_idx_ = 0;
    main_next_idx_ = 0;
    sub_next_idx_ = 0;
}

bool HTTPConnection::ReceiveMessage() {
    if (read_next_idx_ >= read_buffer_max_len_) {
        return false;
    }
    int byte = 0;
    if (Config::GetSingleton_()->connection_level_trigger_mode_) {  // 水平触发。
        byte = recv(client_fd_, read_buffer_ + read_next_idx_, read_buffer_max_len_ - read_next_idx_, 0);
        read_next_idx_ += byte;
        for(int i = 0; i < read_next_idx_; ++i){
            std::cout << read_buffer_[i];
        }
        std::cout << std::endl;
            if (byte <= 0) {
            std::cout << byte << std::endl;
            return false;
        }
    } else {
        while (true) {
            byte = recv(client_fd_, read_buffer_ + read_next_idx_, read_buffer_max_len_ - read_next_idx_, 0);
            if (byte == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
            } else if (byte == 0) {
                return false;
            }
            read_next_idx_ += byte;
        }
    }
    return true;
}