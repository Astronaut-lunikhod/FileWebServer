//
// Created by diode on 23-6-29.
// 多线程部分。
//

#include "ThreadPool.h"


ThreadPool *ThreadPool::singleton_ = nullptr;
pthread_mutex_t  ThreadPool::singleton_mutex_ = PTHREAD_MUTEX_INITIALIZER;

/**
 * 获取线程池的静态单例对象。
 * @return
 */
ThreadPool *ThreadPool::get_singleton_() {
    if(nullptr == singleton_) {
        pthread_mutex_lock(&singleton_mutex_);
        if(nullptr == singleton_) {
            singleton_ = new ThreadPool();
        }
        pthread_mutex_unlock(&singleton_mutex_);
    }
    return singleton_;
}

void *ThreadPool::work(void *args) {
    ThreadPool *pool = (ThreadPool *) args;
    pool->run();
    return pool;
}

/**
 * 最核心的运行函数，多线程会卡在这里面一直执行。
 */
void ThreadPool::run() {
    while (true) {
        sem_wait(&work_queue_sem_);
        pthread_mutex_lock(&work_queue_mutex_);
        if (work_queue_.empty()) {
            pthread_mutex_unlock(&work_queue_mutex_);
            continue;
        }
        HTTPConnection *work = work_queue_.front();
        work_queue_.pop_front();
        pthread_mutex_unlock(&work_queue_mutex_);
        if (!work) {
            continue;
        }
        if (reactor_) {  // 如果是reactor模式进行处理，读写都要子线程完成，所以当前线程需要read、process、write。
            if (work->work_mode_ == HTTPConnection::WORK_MODE::READ) {
                if(work->ReceiveMessage()) {
                    work->Process();  // 如果处理过程中报错，连接会被关闭，如果成功，会触发写事件。
                } else {
                    Utils::DelEpoll(work->client_fd_, work->epoll_fd_);
                    WebServer::connection_num_--;
                }
            } else {
                work->WriteHTTPMessage();  // 成功了会自动激活读事件，失败了自动删除。
            }
        } else {  // proactor模式进行处理，只需要进行逻辑处理。
            work->Process();  // 如果处理过程中报错，连接会被关闭，如果成功，会触发写事件。
        }
    }
}


ThreadPool::ThreadPool() {
    thread_num_ = Config::get_singleton_()->thread_pool_thread_num_;
    threads_ = new pthread_t[thread_num_];
    int ret = -1;
    for (int i = 0; i < thread_num_; ++i) {
        ret = pthread_create(threads_ + i, nullptr, work, this);
        assert(ret == 0);
        ret = pthread_detach(threads_[i]);
        assert(ret != 1);
    }
    work_queue_max_size_ = Config::get_singleton_()->thread_pool_work_queue_max_size_;
    pthread_mutex_init(&work_queue_mutex_, nullptr);
    sem_init(&work_queue_sem_, 0, 0);
    reactor_ = Config::get_singleton_()->thread_pool_reactor_;
}

ThreadPool::~ThreadPool() {
    delete[] threads_;
    pthread_mutex_destroy(&work_queue_mutex_);
    sem_destroy(&work_queue_sem_);
}

/**
 * 向工作队列中插入一个任务，类型是work_mode。
 * @param work
 * @param work_mode
 */
void ThreadPool::append(HTTPConnection *work, int work_mode) {
    if(!work) {
        return;
    }
    pthread_mutex_lock(&work_queue_mutex_);
    if(work_queue_.size() >= work_queue_max_size_) {
        pthread_mutex_unlock(&work_queue_mutex_);
        return;
    }
    work->work_mode_ = (HTTPConnection::WORK_MODE)work_mode;
    work_queue_.push_back(work);
    pthread_mutex_unlock(&work_queue_mutex_);
    sem_post(&work_queue_sem_);
}