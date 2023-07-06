//
// Created by diode on 23-6-29.
// 主线程调用的处理逻辑类。
//

#include "WebServer.h"

unsigned WebServer::connection_num_ = 0;

/**
 * 建立连接
 */
void WebServer::EstablishConnection() {
    sockaddr_in client_address;
    int client_address_size = sizeof(client_address);
    do {
        int client_fd = accept(server_fd_, (sockaddr *) &client_address, (socklen_t *) &client_address_size);
        if (client_fd < 0) {
            return;
        }
        if (connection_num_ >= connection_max_num_) {
            return;
        }

        redisContext *context = nullptr;
        std::string session = "";
        if (Redis::get_singleton_()->GetConnection(&context)) {  // 申请redis连接，准备插入内容。如果连接建立失败，没有后续操作。
            session = Redis::get_singleton_()->SessionExists(context, inet_ntoa(
                    client_address.sin_addr));  // 找出目标ip地址的session字符串。
            Redis::get_singleton_()->ReleaseConnection(context);  // 用完立即释放掉。
            if (session_map_.find(session) == session_map_.end()) {  // 某一个IP第一次连接服务器,创建对应的HTTP对象，这个IP地址以后只能使用这一个对象。
                session_map_[session].insert(client_fd);
            }
        }
        connections_[client_fd].Establish(client_fd, epoll_fd_, client_address, session);
        session_map_[session].insert(client_fd);  // 不管怎样，先将所有同一个IP的请求，绑定到一起。
        Utils::AddEpoll(client_fd, epoll_fd_, true, connection_level_trigger_, connection_one_shot_);
    } while (!listen_level_trigger_);  // 如果是水平触发，监测一次。否则就监测多次。
}

/**
 * 处理读信号。
 */
void WebServer::DealRead(int client_fd) {
    if (reactor_) {  // reactor模式，任务全部交给线程池。
        ThreadPool::get_singleton_()->append(connections_ + client_fd, HTTPConnection::WORK_MODE::READ);
    } else {  // proactor,读写主线程处理，逻辑处理交给线程池。
        if (connections_[client_fd].ReceiveMessage()) {  // 接收成功以后，将处理逻辑的任务交给子线程。
            ThreadPool::get_singleton_()->append(connections_ + client_fd, HTTPConnection::WORK_MODE::PROCESS);
        } else {  // 接收的过程中出现了错误，那肯定要关闭这个套接字。
            Utils::DelEpoll(client_fd, epoll_fd_);
            connections_--;
        }
    }
}

/**
 * 处理写信号。
 * @param client_fd
 */
void WebServer::DealWrite(int client_fd) {
    if (reactor_) {
        ThreadPool::get_singleton_()->append(connections_ + client_fd,
                                             HTTPConnection::WORK_MODE::WRITE);  // reactor模式，将写事件交给子线程。
    } else {
        connections_[client_fd].WriteHTTPMessage();  // 成功了会自动激活读事件，失败了自动删除。
    }
}

WebServer::WebServer() {
    server_fd_ = -1;
    epoll_fd_ = -1;
    elegant_close_ = Config::get_singleton_()->web_server_elegant_close_;
    reuse_ = Config::get_singleton_()->web_server_reuse_;
    server_port_ = Config::get_singleton_()->web_server_server_port_;
    listen_queue_max_size_ = Config::get_singleton_()->web_server_listen_queue_max_size_;
    epoll_meanwhile_listen_max_num = Config::get_singleton_()->web_server_epoll_meanwhile_listen_max_num_;
    listen_level_trigger_ = Config::get_singleton_()->web_server_listen_level_trigger_;
    connection_level_trigger_ = Config::get_singleton_()->web_server_connection_level_trigger_;
    listen_one_shot_ = Config::get_singleton_()->web_server_listen_one_shot_;
    connection_one_shot_ = Config::get_singleton_()->web_server_connection_one_shot_;
    epoll_events_max_size_ = Config::get_singleton_()->web_server_epoll_events_max_size_;
    events_ = new epoll_event[epoll_events_max_size_];
    connection_max_num_ = Config::get_singleton_()->web_server_connection_max_num_;
    connections_ = new HTTPConnection[connection_max_num_];
    reactor_ = Config::get_singleton_()->thread_pool_reactor_;
}

WebServer::~WebServer() {
    if (server_fd_ > 0)
        close(server_fd_);
    if (epoll_fd_ > 0)
        close(epoll_fd_);
    delete[] events_;
    delete[] connections_;
}

/**
 * 开始监听目标端口，并开启epoll进行IO多路复用。
 */
void WebServer::InitialConnection() {
    server_fd_ = socket(PF_INET, SOCK_STREAM, 0);
    assert(server_fd_ > 0);
    if (elegant_close_) {
        struct linger l = {1, 1};
        setsockopt(server_fd_, SOL_SOCKET, SO_LINGER, &l, sizeof(l));
    }
    if (reuse_) {
        int tmp = 1;
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(tmp));
    }
    sockaddr_in server_address;
    server_address.sin_family = PF_INET;
    server_address.sin_port = htons(server_port_);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    int ret = 0;
    ret = bind(server_fd_, (sockaddr *) &server_address, (socklen_t) sizeof(server_address));
    assert(ret != -1);
    ret = listen(server_fd_, listen_queue_max_size_);
    assert(ret != -1);
    epoll_fd_ = epoll_create(epoll_meanwhile_listen_max_num);
    assert(epoll_fd_ != -1);
    // 使用epoll监听server_fd
    Utils::AddEpoll(server_fd_, epoll_fd_, true, listen_level_trigger_, listen_one_shot_);
}

/**
 * 重复对Epoll的结果进行处理。
 */
void WebServer::EpollLoop() {
    bool exists = false;
    while (!exists) {
        int count = epoll_wait(epoll_fd_, events_, epoll_events_max_size_, -1);
        assert(!(count < 0 && errno != EINTR));
        for (int i = 0; i < count; ++i) {
            int fd = events_[i].data.fd;
            if (fd == server_fd_) {  // 建立连接。
                EstablishConnection();
            } else if (events_[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {  // 出错了
                Utils::DelEpoll(fd, epoll_fd_);
                connection_num_--;
            } else if (events_[i].events & EPOLLIN) {  // 读事件已经就绪。
                for (auto ite = session_map_[connections_[fd].session_].begin();
                     ite != session_map_[connections_[fd].session_].end(); ++ite) {  // 循环遍历所有的连接的登录状态。
                    if(connections_[*ite].login_state_) {  // 如果有登录了的，将所有的都改为登录状态。
                        for (int jte : session_map_[connections_[fd].session_]) {
                            if(!connections_[jte].login_state_)
                                connections_[jte] = connections_[*ite];
                        }
                        break;
                    }
                }
                DealRead(fd);
            } else if (events_[i].events & EPOLLOUT) {  // 写事件已经就绪。
                DealWrite(fd);
                for (int jte : session_map_[connections_[fd].session_]) {  // 如果有类似于back的请求，那需要修改相关套接字的pwd_状态。
                        connections_[jte] = connections_[fd];
                }
            }
        }
    }
}