//
// Created by diode on 23-6-29.
// 通用的工具类。
//

#ifndef FILEWEBSERVER_UTILS_H
#define FILEWEBSERVER_UTILS_H

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <dirent.h>
#include <vector>

class WebServer;

class Utils {
public:
    /**
     * 设置fd为非阻塞的。
     * @param fd
     */
    static void SetNonBlocking(int fd) {
        int old_option = fcntl(fd, F_GETFL);
        int new_option = old_option | O_NONBLOCK;
        fcntl(fd, F_SETFL, new_option);
    }

    /**
     * 将fd添加到epoll监听中。
     * @param fd
     * @param epoll_fd
     * @param read true为读，false为写。
     * @param level_trigger true为水平触发，false为边缘。
     * @param one_shot 是否使用one_shot
     */
    static void AddEpoll(int fd, int epoll_fd, bool read, bool level_trigger, bool one_shot) {
        epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLRDHUP;
        if (read) {
            event.events |= EPOLLIN;
        } else {
            event.events |= EPOLLOUT;
        }
        if (!level_trigger) {  // 默认是水平，所以只要监测边缘触发就可以。
            event.events |= EPOLLET;
        }
        if (one_shot) {
            event.events |= EPOLLONESHOT;
        }
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
        SetNonBlocking(fd);
    }

    /**
     * 删除epoll对于fd的监听。
     * @param fd
     * @param epoll_fd
     */
    static void DelEpoll(int fd, int epoll_fd) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, 0);
        close(fd);
    }

    /**
     * 修改epoll中对于fd的监听事件。
     * @param fd
     * @param epoll_fd
     * @param read
     * @param level_trigger
     * @param one_shot
     */
    static void ModEpoll(int fd, int epoll_fd, bool read, bool level_trigger, bool one_shot) {
        struct epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLRDHUP;
        if (read) {
            event.events |= EPOLLIN;
        } else {
            event.events |= EPOLLOUT;
        }
        if (!level_trigger) {
            event.events |= EPOLLET;
        }
        if (one_shot) {
            event.events |= EPOLLONESHOT;
        }
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
    }

    /**
     * 遍历当前的目录，并给出所有文件的名字以及对应的权限。
     * @param dir_path
     * @param names
     * @param share
     */
    static void IteratorDir(const std::string &dir_path, std::vector<std::string> &names, std::vector<bool> &is_dirs,
                            std::vector<bool> &shares) {
        DIR *dir = opendir(dir_path.c_str());
        assert(dir != nullptr);

        struct dirent *entry;
        struct stat file_stat;
        while ((entry = readdir(dir)) != nullptr) {
            std::string file_name = entry->d_name;
            if (file_name == "." || file_name == "..") {
                continue;
            }
            names.emplace_back(file_name);
            std::string file_path = dir_path + "/" + file_name;
            assert(stat(file_path.c_str(), &file_stat) != -1);  // 取出权限。
            if (S_ISREG(file_stat.st_mode)) {
                is_dirs.emplace_back(false);
                if ((file_stat.st_mode & (0777)) & S_IXOTH) {  // 其他人可执行。
                    shares.emplace_back(true);
                } else {
                    shares.emplace_back(false);
                }
            } else if (S_ISDIR(file_stat.st_mode)) {
                is_dirs.emplace_back(true);
                if ((file_stat.st_mode & (0777)) & S_IXOTH) {  // 其他人可执行。
                    shares.emplace_back(true);
                } else {
                    shares.emplace_back(false);
                }
            };
        }
    }

    /**
     * 删除一个文件夹不管是否有内容。
     * @param dir_path
     */
    static void DeleteDir(const std::string &dir_path) {
        DIR *dir = opendir(dir_path.c_str());
        assert(dir != nullptr);
        struct dirent* entry;
        while((entry = readdir(dir)) != nullptr) {
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            std::string entryPath = dir_path + "/" + entry->d_name;
            struct stat st;
            if(stat(entryPath.c_str(), &st) == -1) {
                continue;
            }
            if(S_ISDIR(st.st_mode)) {
                DeleteDir(entryPath);
            } else {
                unlink(entryPath.c_str());
            }
        }
        closedir(dir);
        rmdir(dir_path.c_str());
    }
};

#endif