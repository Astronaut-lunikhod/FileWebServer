//
//  Created by diode on 23-6-29.
//  线程池类。
//

#ifndef FILEWEBSERVER_THREADPOOL_H
#define FILEWEBSERVER_THREADPOOL_H

#include <pthread.h>
#include <list>
#include <semaphore.h>
#include "../HTTPConnection/HTTPConnection.h"
#include "../Config/Config.h"
class HTTPConnection;

class ThreadPool {
private:
    int thread_num_;  // 线程数量。
    int max_request_queue_size_;  // 工作队列的最大长度。
    pthread_t *threads_;  // 线程数组。
    std::list<HTTPConnection *> work_queue_;  // 工作队列
    pthread_mutex_t work_mutex_;
    sem_t work_sem_;
    bool reactor_;

    static ThreadPool *thread_pool_singleton_;
    static pthread_mutex_t singleton_mutex;
public:
    ThreadPool();

    ~ThreadPool();

    static void *worker(void *args);

    void run();

    bool append(HTTPConnection *request, int workMode);

    static ThreadPool *GetSingleton();
};

#endif