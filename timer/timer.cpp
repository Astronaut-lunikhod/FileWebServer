//
// Created by diode on 23-7-6.
//

#include "timer.h"

/**
 * 升序链表的构造函数。
 */
sort_timer_list::sort_timer_list() {
    head = nullptr;
    tail = nullptr;
}

sort_timer_list::~sort_timer_list() {
    Timer *tmp = head;
    while (tmp) {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

/**
 * 往升序链表中插入一个新的定时器。
 * @param timer
 */
void sort_timer_list::AddTimer(Timer *timer) {
    if (nullptr == timer) {
        return;
    }
    if (nullptr == head) {
        head = tail = timer;
        return;
    }
    if (timer->expire_ < head->expire_) {  // 时间比头还小，直接插在头上。
        timer->next = head;
        head->pre = timer;
        head = timer;
        return;
    }
    InsertTime(timer, head);
}

/**
 * 因为不能直接判断插在那里，所以需要从mid开始搜索，往后看，可以插入哪里。
 * @param timer
 * @param mid
 */
void sort_timer_list::InsertTime(Timer *timer, Timer *mid) {
    if (!timer) {  // timer不能是空
        return;
    }
    Timer *pre = mid, *post = mid->next;
    while (nullptr != post) {
        if (timer->expire_ < post->expire_) {  // 可以插入。
            pre->next = timer;
            timer->next = post;
            post->pre = timer;
            timer->pre = pre;
            return;
        }
        pre = post;
        post = post->next;
    }
    if (nullptr == post) {  // 说明已经遍历到了tail之后，当前的过期时间是最大的。
        tail->next = timer;
        timer->pre = tail;
        timer->next = nullptr;
        tail = timer;
    }
}

/**
 * 调整定时器在链表中的位置。
 * @param timer
 */
void sort_timer_list::AdjustTimer(Timer *timer) {
    if (!timer) {
        return;
    }
    if (timer == tail) {  // 要调整的定时器在结尾的地方，那就没有要调整的必要了。
        return;
    }
    if (timer->expire_ < timer->next->expire_) {  // 调整以后还是小，那就没有必要调整。
        return;
    }
    // 能到达这里，说明肯定是需要调整位置的。
    if (timer == head) {
        head = head->next;
        head->pre = nullptr;
        timer->next = nullptr;
        AddTimer(timer);  // 重新插入位置。
    } else {  // 定时器在head和tail中间,取出这个定时器，进行重新插入。
        timer->pre->next = timer->next;
        timer->next->pre = timer->pre;
        InsertTime(timer, timer->next);
    }
}

/**
 * 删除定时器。
 * @param timer
 */
void sort_timer_list::DelTimer(Timer *timer) {
    if (nullptr == timer) {
        return;
    }
    if (timer == head && timer == tail) {
        delete timer;
        head = nullptr;
        tail = nullptr;
        return;
    }
    if (timer->pre == nullptr) {
        head = head->next;
        head->pre = nullptr;
        delete timer;
        return;
    }
    if (timer->next == nullptr) {
        tail = tail->pre;
        tail->next = nullptr;
        delete timer;
        return;
    }
    // 能到这里，说明删除的部分在中间。
    timer->pre->next = timer->next;
    timer->next->pre = timer->pre;
    delete timer;
}

/**
 * 删除所有的过期定时器。
 */
void sort_timer_list::Tick(int epoll_fd) {
    Log::get_log_singleton_instance_()->write_log(Log::LOG_LEVEL::INFO, "进行一次定时器检查。");
    if (!head) {  // 没有内容直接结束。
        return;
    }
    time_t cur = time(nullptr);
    Timer *tmp = head;
    while (tmp) {
        if (cur < tmp->expire_) {  // 停止过期，也即tmp还没有到期。
            break;
        }
        Log::get_log_singleton_instance_()->write_log(Log::LOG_LEVEL::WARNING, "服务器关闭了与%s的一个连接",
                                                      inet_ntoa(tmp->con_.client_address.sin_addr));
        // 关闭对应的文件描述符的监听事件，以及关闭文件描述。其实也就是DelEpoll。Del固定的三步。
        Utils::DelEpoll(tmp->con_.socket_fd_, epoll_fd);
        WebServer::connection_num_--;
        pthread_mutex_lock(&WebServer::session_map_mutex_);
        WebServer::session_map_[WebServer::connections_[tmp->con_.socket_fd_].session_].erase(
                tmp->con_.socket_fd_);  // 断开连接以后需要解除绑定。
        pthread_mutex_unlock(&WebServer::session_map_mutex_);
        head = tmp->next;
        if (head) {  // 判断删除以后是否为空，如果为空，那就不用设置前缀了。
            head->pre = nullptr;
        }
        delete tmp;
        tmp = head;
    }
}