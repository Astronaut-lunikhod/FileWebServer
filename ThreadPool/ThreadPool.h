//
// Created by diode on 23-6-29.
// 多线程部分。
//

#ifndef FILEWEBSERVER_THREADPOOL_H
#define FILEWEBSERVER_THREADPOOL_H

#include <pthread.h>
#include <semaphore.h>
#include <cassert>
#include <list>
#include "../HTTPConnection/HTTPConnection.h"
#include "../Config/Config.h"
#include "../Utils/Utils.h"

class HTTPConnection;

class ThreadPool {
private:
    unsigned thread_num_;  // 线程个数。
    pthread_t *threads_;  // 线程数组。
    unsigned work_queue_max_size_;  // 工作队列中元素的最大个数。
    std::list<HTTPConnection *> work_queue_;  // 工作队列,以及对应的两个索。
    pthread_mutex_t work_queue_mutex_;
    sem_t work_queue_sem_;
    bool reactor_;  // 是否使用reactor模式进行工作。

    static ThreadPool *singleton_;
    static pthread_mutex_t singleton_mutex_;
public:
    static ThreadPool *get_singleton_();

private:
    static void *work(void *args);

    void run();  // 最核心的运行函数，多线程会卡在这里面一直执行。

    ThreadPool();

    ~ThreadPool();

public:
    void append(HTTPConnection *work, int work_mode);  // 向工作队列中插入一个任务，类型是work_mode。
};

#endif