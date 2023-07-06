//
//  Created by diode on 23-6-25.
//  日志类的实现类。
//

#include "Log.h"

Log *Log::log_singleton_instance_ = nullptr;
pthread_mutex_t Log::singleton_instance_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * 创建静态单例对象。
 * @param open_async 日志类的一些参数。
 * @return
 */
Log *Log::get_log_singleton_instance_() {
    if (nullptr == log_singleton_instance_) {
        pthread_mutex_lock(&singleton_instance_mutex);
        if (nullptr == log_singleton_instance_) {
            log_singleton_instance_ = new Log();
        }
        pthread_mutex_unlock(&singleton_instance_mutex);
    }
    return log_singleton_instance_;
}

/**
 * 使用给定的默认参数构造Log类。
 */
Log::Log() {
    this->open_log_ = Config::get_singleton_()->open_log_;
    if (!this->open_log_) {  // 不打开日志功能的话，后面没有必要继续操作。
        return;
    }
    this->open_async_ = Config::get_singleton_()->open_async_log_;  // 是否打开异步写入日志的功能。
    this->log_buffer_max_size_ = Config::get_singleton_()->log_buffer_max_size_;  // 日志缓冲的最大内容。
    this->log_buffer_ = new char[this->log_buffer_max_size_];  // 日志的缓冲数组。
    memset(this->log_buffer_, '\0', this->log_buffer_max_size_);
    this->max_lines_ = Config::get_singleton_()->log_max_lines_;  // 一份日志最多有多少行数据。
    this->now_lines_ = 0;  // 当前还未记录,所以日志行数为0。
    strcpy(this->dir_name_, Config::get_singleton_()->log_dir_name_);  // 记录日志文件的路径以及名字。
    strcpy(this->log_name_, Config::get_singleton_()->log_file_name_);

    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
    std::tm *local_time = std::localtime(&timestamp);  // 获取当日的时间格式，用来创建日志文件的名字。

    char log_full_name[256] = {'\0'};
    snprintf(log_full_name, 255, "%s/%d_%02d_%02d_%s-0", this->dir_name_, local_time->tm_year + 1900,
             local_time->tm_mon + 1, local_time->tm_mday, this->log_name_);  // 构造日志文件的全路径,这时候启动的一定是结尾为-0的文件。
    this->log_day_ = local_time->tm_mday;  // 记录今天的时间
    this->log_fp_.open(log_full_name, std::ios::app);  // 打开一个文件的句柄。
    pthread_mutex_init(&write_log_mutex_, nullptr);  // 写日志的互斥锁。
    if (this->open_async_) {  // 如果需要开启异步记录日志，那么开启一个额外的线程。
        log_queue_ = new BlockQueue<std::string>(Config::get_singleton_()->log_max_queue_num);
        pthread_t log_thread;
        pthread_create(&log_thread, nullptr, static_log_thread_start, nullptr);
    }
}

/**
 * 必须要使用静态的函数，才可以避免一些问题的发生，但是其中可以调用非静态函数，达到转折的效果。
 * @param args
 * @return
 */
void *Log::static_log_thread_start(void *args) {
    Log::get_log_singleton_instance_()->async_write_log();
}

/**
 * 开始异步写日志
 * @return
 */
void *Log::async_write_log() {
    std::string log;
    while ((this->log_queue_->pop(log))) {  // 和BlockQueue中的pop进行联动，真正的成为了消费者，其他的工作线程都是生产者，生产日志，这个线程专门消费。
        pthread_mutex_lock(&write_log_mutex_);
        this->log_fp_ << log << std::endl;  // 将内容写到对应的句柄文件中。
        pthread_mutex_unlock(&write_log_mutex_);
    }
}

/**
 * 将新的内容写到消息队列中，以方便消费者将消息取出来。
 * @param level
 * @param ...
 */
void Log::write_log(int level, const char *format, ...) {
    char log_level[16]{'\0'};
    switch (level) {
        case LOG_LEVEL::DEBUG:
            strcpy(log_level, "[debug]:");
            break;
        case LOG_LEVEL::INFO:
            strcpy(log_level, "[info]:");
            break;
        case LOG_LEVEL::WARNING:
            strcpy(log_level, "[warning]:");
            break;
        case LOG_LEVEL::ERROR:
            strcpy(log_level, "[error]:");
            break;
        default:
            strcpy(log_level, "[error]:");
    }
    pthread_mutex_lock(&write_log_mutex_);
    now_lines_++;  // 因为要新增行，所以增加一下。
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
    std::tm *local_time = std::localtime(&timestamp);  // 获取当日的时间格式，用来创建日志文件的名字。
    if (local_time->tm_mday != this->log_day_ ||
        this->now_lines_ % this->max_lines_ == 0) {  // 如果日期已经发生了变化或者日志已经到了最大的行数，需要打开一个新的文件。
        this->log_fp_.flush();
        this->log_fp_.close();  // 先将没有录入的内容全部录入，之后再关闭这个句柄。
        char log_full_name[256]{'\0'};
        if (this->log_day_ != local_time->tm_mday) {  // 如果是日期变化了，那就要改变名字中的日期。
            snprintf(log_full_name, 255, "%s/%d_%02d_%02d_%s", this->dir_name_, local_time->tm_year + 1900,
                     local_time->tm_mon + 1, local_time->tm_mday, this->log_name_);  // 构造日志文件的全路径。
            this->log_day_ = local_time->tm_mday;
            this->now_lines_ = 0;
        } else {  // 在文件的后头加上序号。
            snprintf(log_full_name, 255, "%s/%d_%02d_%02d_%s-%d", this->dir_name_, local_time->tm_year + 1900,
                     local_time->tm_mon + 1, local_time->tm_mday, this->log_name_,
                     this->now_lines_ / this->max_lines_);  // 构造日志文件的全路径。
        }
        this->log_fp_.open(log_full_name, std::ios::app);  // 重新打开目标的句柄文件
    }
    va_list va_list;  // 以下开始读取可变参数列表
    va_start(va_list, format);
    std::string log_content;
    int index = snprintf(this->log_buffer_, this->log_buffer_max_size_ - 1, "%d-%02d-%02d %02d:%02d:%02d %s",
                         local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday, local_time->tm_hour,
                         local_time->tm_min, local_time->tm_sec, log_level);  // 注意，总size一定要减少1，因为要留一个給'\0'
    int m = vsnprintf(this->log_buffer_ + index, this->log_buffer_max_size_ - index - 1, format,
                      va_list);
    this->log_buffer_[m + index + 1] = '\0';  // 这里本来还要写一个\n，但是因为有endl，不仅换行，还清除了行缓冲，所以就不写endl了。
    log_content = this->log_buffer_;  // 这就是构造好的最终日志，要记录到日志文件中，只需要写道阻塞队列里面就行了。
    if(this->open_async_) {  // 运行异步的情况下，可以直接将内容插入到阻塞队列。
        this->log_queue_->push(log_content);
    } else {
        this->log_fp_ << log_content << std::endl;
    }
    pthread_mutex_unlock(&this->write_log_mutex_);
    va_end(va_list);
}