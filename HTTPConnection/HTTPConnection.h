//
//  Created by diode on 23-6-29.
//  每一个连接对应一个HTTPConnection对象。
//

#ifndef FILEWEBSERVER_HTTPCONNECTION_H
#define FILEWEBSERVER_HTTPCONNECTION_H

#include <sys/socket.h>
#include <netinet/in.h>
#include "../WebServer/WebServer.h"
#include "../Config/Config.h"


class HTTPConnection {
public:
    int client_fd_;
    int epoll_fd_;
    sockaddr_in client_address_;
    char *read_buffer_;
    int read_buffer_max_len_;
    int read_next_idx_;
    int main_next_idx_;
    int sub_next_idx_;

public:
    enum WORK_MODE {  // 当前的连接应该进行什么操作，读，处理数据，还是写。
        READ,
        PROCESS,
        WRITE
    };

    WORK_MODE work_mode_;

    HTTPConnection() {};

    ~HTTPConnection() {};

    void EstablishConnection(int client_fd, int epoll_fd, const sockaddr_in &client_address);

    void Init();

    bool ReceiveMessage();


};

#endif
