//
//  Created by diode on 23-6-29.
//

#include <cassert>
#include <iostream>
#include "ThreadPool.h"
#include "../Utils/Utils.h"

ThreadPool* ThreadPool::thread_pool_singleton_ = nullptr;
pthread_mutex_t ThreadPool::singleton_mutex = PTHREAD_MUTEX_INITIALIZER;

ThreadPool *ThreadPool::GetSingleton() {
    if(nullptr == thread_pool_singleton_) {
        pthread_mutex_lock(&singleton_mutex);
        if(nullptr == thread_pool_singleton_) {
            thread_pool_singleton_ = new ThreadPool();
        }
        pthread_mutex_unlock(&singleton_mutex);
    }
    return thread_pool_singleton_;
}

ThreadPool::ThreadPool() {
    pthread_mutex_init(&work_mutex_, nullptr);
    sem_init(&work_sem_, 0, 0);
    thread_num_ = Config::GetSingleton_()->thread_num_;
    max_request_queue_size_ = Config::GetSingleton_()->max_request_queue_size_;
    reactor_ = Config::GetSingleton_()->reactor_;
    threads_ = new pthread_t [thread_num_];
    for(int i = 0; i < thread_num_; ++i) {
        int ret = pthread_create(threads_ + i, nullptr, worker, this);  /// why
        assert(ret == 0);
        ret = pthread_detach(threads_[i]);
        assert(ret != 1);
    }
}

ThreadPool::~ThreadPool() {
    delete [] threads_;
}

void *ThreadPool::worker(void *args) {
    ThreadPool *pool = (ThreadPool *)args;
    pool->run();
    return pool;
}

void ThreadPool::run() {
    while(true) {
        sem_wait(&work_sem_);
        pthread_mutex_lock(&work_mutex_);
        if(work_queue_.empty()) {
            pthread_mutex_unlock(&work_mutex_);
            continue;
        }
        HTTPConnection *request = work_queue_.front();
        work_queue_.pop_front();
        pthread_mutex_unlock(&work_mutex_);
        if(!request) {  // 判断请求是否合理。
            continue;
        }
        if(reactor_) {  // 如果是reactor模式，读写的任务也交给工作队列。
            if(request->work_mode_ == HTTPConnection::WORK_MODE::READ) {
                if(request->ReceiveMessage()) {
                    std::cout << "处理操作" << std::endl;
                    Utils::ModEpollFd(request->client_fd_, request->epoll_fd_, false,
                                      Config::GetSingleton_()->connection_level_trigger_mode_,
                                      Config::GetSingleton_()->one_shot_);
                } else {
                    Utils::DelEpollFd(request->client_fd_, request->epoll_fd_);
                }
            } else {
                std::cout << "发送操作。" << std::endl;
                std::string line = "HTTP/1.1 200 OK\r\n" "Content-Type: text/html; charset=utf-8\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
                send(request->client_fd_, line.c_str(), line.size(), 0);
                request->Init();
                Utils::ModEpollFd(request->client_fd_, request->epoll_fd_, true, Config::GetSingleton_()->connection_level_trigger_mode_, Config::GetSingleton_()->one_shot_);
            }
        } else {
            std::cout << "处理事件" << std::endl;
            Utils::ModEpollFd(request->client_fd_, request->epoll_fd_, false, Config::GetSingleton_()->connection_level_trigger_mode_, Config::GetSingleton_()->one_shot_);
        }
    }
}

/**
 * 往工作队列中添加内容，但是要设置读任务还是写任务。但如果是proactor模式，是不需要设置读写的，因为读写已经在主线程进行操作。
 * @param request
 * @param read
 * @return
 */
bool ThreadPool::append(HTTPConnection *request, int work_mode) {
    pthread_mutex_lock(&work_mutex_);
    if(work_queue_.size() >= max_request_queue_size_) {
        pthread_mutex_unlock(&work_mutex_);
        return false;
    }
    request->work_mode_ = (HTTPConnection::WORK_MODE)work_mode;
    work_queue_.push_back(request);
    pthread_mutex_unlock(&work_mutex_);
    sem_post(&work_sem_);
    return true;
}