//
// Created by diode on 23-6-28.
//

#ifndef FILEWEBSERVER_UTILS_H
#define FILEWEBSERVER_UTILS_H

#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include "../Config/Config.h"

class Utils {
public:
    Utils() {};

    ~Utils() {};

    static void SetNonBlocking(int fd) {
        int old_option = fcntl(fd, F_GETFL);
        int new_option = old_option | O_NONBLOCK;
        fcntl(fd, F_SETFL, new_option);
    }

    static void AddEpollFd(int fd, int epoll_fd, bool read, bool level_trigger, bool one_shot) {
        struct epoll_event event;
        event.data.fd = fd;
        if (read) {  // 读事件
            event.events = EPOLLIN;
        } else {
            event.events = EPOLLOUT;
        }
        if (!level_trigger) {  // 边缘触发。
            event.events = event.events | EPOLLRDHUP | EPOLLET;
        } else {
            event.events = event.events | EPOLLRDHUP;
        }
        if (one_shot) {
            event.events = event.events | EPOLLONESHOT;
        }
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
        SetNonBlocking(fd);
    }

    static void DelEpollFd(int fd, int epoll_fd) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, 0);
        close(fd);
    }

    static void ModEpollFd(int fd, int epoll_fd, bool read, bool level_trigger, bool one_shot) {
        struct epoll_event event;
        event.data.fd = fd;
        if (read) {  // 读事件
            event.events = EPOLLIN;
        } else {
            event.events = EPOLLOUT;
        }
        if (!level_trigger) {  // 边缘触发。
            event.events = event.events | EPOLLRDHUP | EPOLLET;
        } else {
            event.events = event.events | EPOLLRDHUP;
        }
        if (one_shot) {
            event.events = event.events | EPOLLONESHOT;
        }
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
    }
};

#endif
