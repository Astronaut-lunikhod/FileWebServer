//
// Created by diode on 23-6-29.
// 系统的配置类，每个模块的配置都由Config的懒汉式单例对象提供。
//

#ifndef FILEWEBSERVER_CONFIG_H
#define FILEWEBSERVER_CONFIG_H
#include <pthread.h>
#include <string>

class Config {
private:
    static Config * singleton_;
    static pthread_mutex_t singleton_mutex_;

public:

private:
    Config();

    ~Config();

public:
    static Config *get_singleton_();  // 获取单例模型的静态函数。

    unsigned thread_pool_thread_num_;  // 线程个数。
    unsigned thread_pool_work_queue_max_size_;  // 工作队列中元素的最大个数。
    bool thread_pool_reactor_;  // 是否使用reactor模式进行工作。

    bool web_server_elegant_close_;  // 是否优雅关闭。
    bool web_server_reuse_;  // 是否复用地址。
    unsigned web_server_server_port_;  // 服务器使用的端口
    unsigned web_server_listen_queue_max_size_;  // listen队列中全连接的最大保存数量。
    unsigned web_server_epoll_meanwhile_listen_max_num_;  // epoll同时监测的最大数量。
    bool web_server_listen_level_trigger_;  // 监听建立连接的部分是否使用水平触发。
    bool web_server_connection_level_trigger_;  // 已经建立的连接是否使用水平触发。
    bool web_server_listen_one_shot_;  // 同理，监听和已建立的是否使用one_shot。
    bool web_server_connection_one_shot_;
    unsigned web_server_epoll_events_max_size_;  // 存放epoll事件结果的容器大小。
    unsigned web_server_connection_max_num_;  // 同时建立连接的最大个数。

    unsigned http_connection_read_buffer_max_len_;  // read缓存的大小。
    unsigned http_connection_response_file_path_max_len_;  // 响应的页面保存在服务器上绝对路径的最大长度。
    std::string http_connection_html_dir_path_;  // html文件夹的根路径。
    unsigned http_connection_write_buffer_max_len_;

    std::string mysql_host_;  // 数据库连接五要素。
    std::string mysql_port_;
    std::string mysql_username_;
    std::string mysql_password_;
    std::string mysql_database_name_;
    unsigned mysql_connection_max_num_;  // 数据库连接的最大连接数。

    std::string http_connection_file_dir_root_path_;  // 网盘的根目录。
};
#endif