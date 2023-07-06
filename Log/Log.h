//
//  Created by diode on 23-6-25.
//  日志类。
//

#ifndef CHATROOM_LOG_H
#define CHATROOM_LOG_H

#include <pthread.h>
#include <string>
#include <cstring>
#include <chrono>
#include <cstdarg>
#include "BlockQueue.h"
#include <fstream>
#include "../Config/Config.h"

class Log {
private:
    static Log *log_singleton_instance_;  // 日志类的单例对象。
    static pthread_mutex_t singleton_instance_mutex;  // 创建静态单例对象的静态锁。
    bool open_log_;  // 是否开启日志，true代表开启，false代表不开启。
    bool open_async_;  // 使用异步的方式记录日志,true代表异步，false代表同步。

    Log();

    ~Log() {  // 关闭句柄以及所有的相关锁。
        pthread_mutex_lock(&write_log_mutex_);
        log_fp_.close();
        pthread_mutex_unlock(&write_log_mutex_);
        pthread_mutex_destroy(&singleton_instance_mutex);
        pthread_mutex_destroy(&write_log_mutex_);
    };

    void *async_write_log();  // 异步写日志

    BlockQueue<std::string> *log_queue_;  // 消息队列，届时使用其中的pop函数进行消费，取出元素，然后自己的线程进行处理。
    pthread_mutex_t write_log_mutex_;  // 写日志的时的互斥锁，使用这个是因为同步和异步的写入的日志的时候可能会出现冲突，实际上还是多线程的。
    char dir_name_[128];  // 日志的目录路径
    char log_name_[128];  // 日志的文件名字
    unsigned max_lines_;  // 日志的最大行数。
    unsigned now_lines_;  // 当前已经记录的日志行数。

    unsigned log_buffer_max_size_;  // 日志的最大缓冲区
    char *log_buffer_;  // 日志缓冲，通过将要写入的日志作为元素加入到BlockQueue中，进行生产操作。

    int log_day_;  // 记录今天，因为日志是按照日期进行分类的。
    std::ofstream log_fp_;  // 日志文件的句柄。
public:
    enum LOG_LEVEL {
        DEBUG = 0,
        INFO,
        WARNING,
        ERROR
    };
private:
public:
    static Log *get_log_singleton_instance_();  // 构造懒汉式单例对象的方法。

    static void *static_log_thread_start(void *args);  // 静态的线程启动函数，用于避免this指针传入的问题。

    void write_log(int level, const char *format, ...);  // 将新的内容写入到消息队列中。
};

#endif
