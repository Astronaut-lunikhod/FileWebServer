//
// Created by diode on 23-6-29.
// 系统的配置类，每个模块的配置都由Config的懒汉式单例对象提供。
//

#include "Config.h"

Config *Config::singleton_ = nullptr;
pthread_mutex_t Config::singleton_mutex_ = PTHREAD_MUTEX_INITIALIZER;  // 默认的初始化参数，即便没有调用初始化函数也可以使用了。

Config::Config() {  // 用于赋值各种参数，别的模块可以通过单例模型直接拿到内容。
    thread_pool_thread_num_ = 8;
    thread_pool_work_queue_max_size_ = (1 << 15);
    thread_pool_reactor_ = true;
    web_server_elegant_close_ = true;
    web_server_reuse_ = true;
    web_server_server_port_ = 7777;
    web_server_listen_queue_max_size_ = (1 << 14);
    web_server_epoll_meanwhile_listen_max_num_ = (1 << 14);
    web_server_listen_level_trigger_ = true;
    web_server_connection_level_trigger_ = false;
    web_server_listen_one_shot_ = false;  // 这个是固定的，一定是false。
    web_server_connection_one_shot_ = true;
    web_server_epoll_events_max_size_ = (1 << 14);
    web_server_connection_max_num_ = (1 << 14);
    http_connection_read_buffer_max_len_ = 1024;
    http_connection_response_file_path_max_len_ = 256;
    http_connection_html_dir_path_ = "../HTMLDir";
    http_connection_write_buffer_max_len_ = 1024;
    mysql_host_ = "127.0.0.1";
    mysql_port_ = "3306";
    mysql_username_ = "root";
    mysql_password_ = "";
    mysql_database_name_ = "FileWebServer";
    mysql_connection_max_num_ = 8;
    http_connection_file_dir_root_path_ = "../FileDir";
    http_connection_resource_dir_root_path_ = "../ResourceDir";
    redis_host_ = "localhost";
    redis_port_ = 6379;
    redis_password_ = "123456";
    redis_pool_max_count_ = 8;
    redis_generator_session_length_ = 16;
    open_log_ = true;
    log_buffer_max_size_ = 1024;
    log_max_lines_ = (1 << 20);
    strcpy(log_dir_name_, "../LogDir\0");
    strcpy(log_file_name_, "serverLog\0");
    log_max_queue_num = (1 << 13);
    alarm_interval_ = 5;
}

Config::~Config() {

}

/**
 * 懒汉式加载单例模型。
 * @return
 */
Config *Config::get_singleton_() {
    if (nullptr == singleton_) {
        pthread_mutex_lock(&singleton_mutex_);
        if (nullptr == singleton_) {
            singleton_ = new Config();
        }
        pthread_mutex_unlock(&singleton_mutex_);
    }
    return singleton_;
}

