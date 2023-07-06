//
//  Created by diode on 23-6-25.
//  用来装载消费队列的数据结构,因为是模板类，所以不写实现文件了，直接在头文件中实现。
//

#ifndef CHATROOM_BLOCKQUEUE_H
#define CHATROOM_BLOCKQUEUE_H

#include <pthread.h>
#include <cassert>

template<class T>
class BlockQueue {
private:
    pthread_mutex_t queue_mutex_;  // 使用互斥锁和条件变量，构造一个高速缓存，实现消费者和生产者模型。
    pthread_cond_t queue_cond_;
    T *array_;  // 最终保存数据的数组。
    unsigned max_size_;  // 最大的size。
    unsigned now_size_;  // 当前的数组中保存了了多少元素。
    int start_index_;  // 第一个元素的下标。
    int end_index_;  // 最后一个元素的下标。
public:
    BlockQueue(unsigned max_size) {
        assert(max_size > 0);
        pthread_mutex_init(&queue_mutex_, nullptr);
        pthread_cond_init(&queue_cond_, nullptr);
        this->max_size_ = max_size;
        this->now_size_ = 0;
        this->start_index_ = -1;
        this->end_index_ = -1;
        array_ = new T[this->max_size_];
    }

    ~BlockQueue() {
        pthread_mutex_lock(&queue_mutex_);
        if (nullptr != array_) {
            delete[] array_;
        }
        pthread_mutex_unlock(&queue_mutex_);
        pthread_mutex_destroy(&queue_mutex_);  // 析构的时候记得销毁。
        pthread_cond_destroy(&queue_cond_);
    }

    void clear() {
        pthread_mutex_lock(&queue_mutex_);
        this->now_size_ = 0;
        this->start_index_ = -1;
        this->end_index_ = -1;
        pthread_mutex_unlock(&queue_mutex_);
    }

    bool full() {
        pthread_mutex_lock(&queue_mutex_);
        if (now_size_ == max_size_) {
            pthread_mutex_unlock(&queue_mutex_);
            return true;
        } else {
            pthread_mutex_unlock(&queue_mutex_);
            return false;
        }
    }

    bool empty() {
        pthread_mutex_lock(&queue_mutex_);
        if (now_size_ == 0) {
            pthread_mutex_unlock(&queue_mutex_);
            return true;
        } else {
            pthread_mutex_unlock(&queue_mutex_);
            return false;
        }
    }

    bool front(T &value) {
        pthread_mutex_lock(&queue_mutex_);
        if (!this->empty()) {
            value = array_[start_index_];
        }
        pthread_mutex_unlock(&queue_mutex_);
        return false;
    }

    bool back(T &value) {
        pthread_mutex_lock(&queue_mutex_);
        if (!this->empty()) {
            value = array_[end_index_];
        }
        pthread_mutex_unlock(&queue_mutex_);
        return false;
    }

    int size() {
        int tmp = 0;
        pthread_mutex_lock(&queue_mutex_);
        tmp = this->now_size_;
        pthread_mutex_unlock(&queue_mutex_);
        return tmp;
    }

    bool push(const T &value) {  // 开始生产元素，如果生产成功返回true，否则返回false。
        pthread_mutex_lock(&queue_mutex_);
        if (this->now_size_ == this->max_size_) {  // 已经满了，无法继续生产,抓紧唤醒消费者开始干活。不可使用full()，因为full中也用了互斥锁，冲突了。
            pthread_cond_broadcast(&queue_cond_);
            pthread_mutex_unlock(&queue_mutex_);
            return false;
        }
        this->end_index_ = (this->end_index_ + 1) % this->max_size_;  // 采用循环数组的方式往其中添加信息。
        this->now_size_++;
        array_[end_index_] = value;
        pthread_cond_broadcast(&queue_cond_);
        pthread_mutex_unlock(&queue_mutex_);
        return true;
    }

    bool pop(T &value) {  // 从消息队列中取出内容，返回给线程进行消费。
        pthread_mutex_lock(&queue_mutex_);
        while (this->now_size_ == 0) {  // 不可以使用empty()判断，因为俄empty当中也要用到锁，就很麻烦。
            int ret = pthread_cond_wait(&queue_cond_,
                                        &queue_mutex_);  //  开始等待条件变量的广播，得到广播后，再次锁定，并检查是否为空，不为空就可以退出while循环了。
            assert(ret == 0);  // 如果不为0，代表唤醒的过程存在问题。
        }
        this->start_index_ = (this->start_index_ + 1) % this->max_size_;
        this->now_size_--;
        value = array_[start_index_];
        pthread_mutex_unlock(&queue_mutex_);
        return true;
    }

};

#endif
