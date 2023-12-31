//
// Created by diode on 23-6-29.
// 主线程调用的处理逻辑类。
//

#include "WebServer.h"

unsigned WebServer::connection_num_ = 0;
HTTPConnection *WebServer::connections_;  // 记录连接。
pthread_mutex_t WebServer::session_map_mutex_ = PTHREAD_MUTEX_INITIALIZER;
std::unordered_map<std::string, std::unordered_set<int>> WebServer::session_map_;  // 已经建立连接的对象以及对应的session建立的map。
Timer **WebServer::timers_ = nullptr;
sort_timer_list WebServer::sort_timer_list_;

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
            pthread_mutex_lock(&WebServer::session_map_mutex_);
            if (session_map_.find(session) == session_map_.end()) {  // 某一个IP第一次连接服务器,创建对应的HTTP对象，这个IP地址以后只能使用这一个对象。
                session_map_[session].insert(client_fd);
            }
            pthread_mutex_unlock(&WebServer::session_map_mutex_);
        }
        connections_[client_fd].Establish(client_fd, epoll_fd_, client_address, session);
        pthread_mutex_lock(&WebServer::session_map_mutex_);
        session_map_[session].insert(client_fd);  // 不管怎样，先将所有同一个IP的请求，绑定到一起。
        pthread_mutex_unlock(&WebServer::session_map_mutex_);
        timers_[client_fd] = new Timer();
        timers_[client_fd]->con_.socket_fd_ = client_fd;  // 生成对应的计时器。
        timers_[client_fd]->con_.session_ = session;
        timers_[client_fd]->con_.client_address = client_address;
        timers_[client_fd]->expire_ = time(nullptr) + 3 * Config::get_singleton_()->alarm_interval_;
        sort_timer_list_.AddTimer(timers_[client_fd]);  // 将刚刚形成的计时器插入有序链表中。

        Utils::AddEpoll(client_fd, epoll_fd_, true, connection_level_trigger_, connection_one_shot_);
    } while (!listen_level_trigger_);  // 如果是水平触发，监测一次。否则就监测多次。
}

/**
 * 处理读信号。
 */
void WebServer::DealRead(int client_fd) {
    Timer *timer = timers_[client_fd];
    if (reactor_) {  // reactor模式，任务全部交给线程池。
        if (timer) {
            sort_timer_list_.AdjustTimer(timer);
        }
        ThreadPool::get_singleton_()->append(connections_ + client_fd, HTTPConnection::WORK_MODE::READ);
    } else {  // proactor,读写主线程处理，逻辑处理交给线程池。
        if (connections_[client_fd].ReceiveMessage()) {  // 接收成功以后，将处理逻辑的任务交给子线程。
            ThreadPool::get_singleton_()->append(connections_ + client_fd, HTTPConnection::WORK_MODE::PROCESS);
            if (timer) {
                sort_timer_list_.AdjustTimer(timer);
            }
        } else {  // 接收的过程中出现了错误，那肯定要关闭这个套接字。
            sort_timer_list_.DelTimer(timer);
            Utils::DelEpoll(client_fd, epoll_fd_);
            pthread_mutex_lock(&session_map_mutex_);
            session_map_[connections_[client_fd].session_].erase(client_fd);  // 断开连接以后需要解除绑定。
            pthread_mutex_unlock(&session_map_mutex_);
            connections_--;
        }
    }
}

/**
 * 处理写信号。
 * @param client_fd
 */
void WebServer::DealWrite(int client_fd) {
    Timer *timer = timers_[client_fd];
    if (reactor_) {
        if (timer) {
            sort_timer_list_.AdjustTimer(timer);
        }
        ThreadPool::get_singleton_()->append(connections_ + client_fd,
                                             HTTPConnection::WORK_MODE::WRITE);  // reactor模式，将写事件交给子线程。
    } else {
        if (connections_[client_fd].WriteHTTPMessage()) {  // 成功了会自动激活读事件，失败了自动删除。
            if (timer) {
                sort_timer_list_.AdjustTimer(timer);
            }
        }
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
    timers_ = new Timer *[Config::get_singleton_()->web_server_connection_max_num_];
}

WebServer::~WebServer() {
    if (server_fd_ > 0)
        close(server_fd_);
    if (epoll_fd_ > 0)
        close(epoll_fd_);
    if (pipe_[0] > 0)
        close(pipe_[0]);
    if (pipe_[1] > 0)
        close(pipe_[1]);
    delete[] events_;
    delete[] connections_;
    delete[] timers_;
    pthread_mutex_destroy(&session_map_mutex_);
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

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipe_);
    Utils::read_sig_fd_ = pipe_[0];
    Utils::send_sig_fd_ = pipe_[1];
    assert(ret != -1 && pipe_[0] > 0 && pipe_[1] > 0);
    Utils::AddEpoll(pipe_[0], epoll_fd_, true, false, false);
    Utils::SetNonBlocking(pipe_[1]);
    Utils::AddSig(SIGPIPE, SIG_IGN, true);
    Utils::AddSig(SIGALRM, Utils::SigHandler, false);
    Utils::AddSig(SIGTERM, Utils::SigHandler, false);
    alarm(Config::get_singleton_()->alarm_interval_);
}

/**
 * 重复对Epoll的结果进行处理。
 */
void WebServer::EpollLoop() {
    bool exit = false, timeout = false;
    while (!exit) {
        int count = epoll_wait(epoll_fd_, events_, epoll_events_max_size_, -1);
        assert(!(count < 0 && errno != EINTR));
        for (int i = 0; i < count; ++i) {
            int fd = events_[i].data.fd;
            if (fd == server_fd_) {  // 建立连接。
                EstablishConnection();
            } else if (events_[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {  // 出错了
                sort_timer_list_.DelTimer(timers_[fd]);  // 删除对应的定时器。
                Utils::DelEpoll(fd, epoll_fd_);
                pthread_mutex_lock(&session_map_mutex_);
                session_map_[connections_[fd].session_].erase(fd);  // 断开连接以后需要解除绑定。
                pthread_mutex_unlock(&session_map_mutex_);
                connection_num_--;
            } else if ((events_[i].events & EPOLLIN) && fd == pipe_[0]) {  // 读端接收到了信号，准备开始接收信息。
                DealSig(fd, exit, timeout);
            } else if (events_[i].events & EPOLLIN) {  // 读事件已经就绪。
                pthread_mutex_lock(&session_map_mutex_);
                for (auto ite = session_map_[connections_[fd].session_].begin();
                     ite != session_map_[connections_[fd].session_].end(); ++ite) {  // 循环遍历所有的连接的登录状态。
                    if (connections_[*ite].login_state_) {  // 如果有登录了的，将所有的都改为登录状态。
                        for (int jte: session_map_[connections_[fd].session_]) {
                            if (!connections_[jte].login_state_)
                                connections_[jte] = connections_[*ite];
                        }
                        break;
                    }
                }
                pthread_mutex_unlock(&session_map_mutex_);
                DealRead(fd);
            } else if (events_[i].events & EPOLLOUT) {  // 写事件已经就绪。
                DealWrite(fd);
                pthread_mutex_lock(&session_map_mutex_);
                for (int jte: session_map_[connections_[fd].session_]) {  // 如果有类似于back的请求，那需要修改相关套接字的pwd_状态。
                    if (jte != fd && 0 <= jte && jte <= Config::get_singleton_()->web_server_connection_max_num_)  // 同一个内容就别操作了，会有溢出的问题。
                        connections_[jte] = connections_[fd];
                }
                pthread_mutex_unlock(&session_map_mutex_);
            }
        }
        if (timeout) {  // 更新升序链表。
            sort_timer_list_.Tick(epoll_fd_);
            alarm(Config::get_singleton_()->alarm_interval_);
            timeout = false;
        }
    }
}

/**
 * 处理alarm信号。
 * @param client_fd
 */
void WebServer::DealSig(int client_fd, bool &exit, bool &timeout) {
    int ret = 0;
    char sig[1024]{'\0'};
    ret = recv(client_fd, sig, sizeof(sig), 0);  // 一次性可能会接收多个信号。
    if (ret <= 0) {
        return;
    } else {
        for (int i = 0; i < ret; ++i) {
            switch (sig[i]) {
                case SIGALRM:
                    timeout = true;
                    break;
                case SIGTERM:
                    exit = true;
                    break;
            }
        }
    }
}