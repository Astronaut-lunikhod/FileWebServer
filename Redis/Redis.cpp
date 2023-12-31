//
// Created by diode on 23-7-5.
// redis的连接池的实现文件。

#include "Redis.h"

Redis *Redis::singleton_ = nullptr;
pthread_mutex_t Redis::singleton_mutex_ = PTHREAD_MUTEX_INITIALIZER;

Redis::Redis() {
    host_ = Config::get_singleton_()->redis_host_;
    port_ = Config::get_singleton_()->redis_port_;
    password_ = Config::get_singleton_()->redis_password_;
    pool_max_count_ = Config::get_singleton_()->redis_pool_max_count_;
    pthread_mutex_init(&pool_mutex_, nullptr);
    pthread_mutex_init(&connection_mutex_, nullptr);
    for (int i = 0; i < pool_max_count_; ++i) {
        redisContext *context = redisConnect(host_.c_str(), port_);
        assert(!(context == nullptr || context->err));
        redisReply *reply = (redisReply *) redisCommand(context, "AUTH %s", password_.c_str());
        assert(!(reply == nullptr || reply->type == REDIS_REPLY_ERROR));
        freeReplyObject(reply);
        pool_.emplace_back(context);
    }
}

Redis::~Redis() {
    pthread_mutex_lock(&pool_mutex_);
    if (!pool_.empty()) {
        for (int i = 0; i < pool_.size(); ++i) {
            redisFree(pool_[i]);
        }
    }
    pthread_mutex_unlock(&pool_mutex_);
    pthread_mutex_destroy(&singleton_mutex_);
    pthread_mutex_destroy(&pool_mutex_);
    pthread_mutex_destroy(&connection_mutex_);
}

Redis *Redis::get_singleton_() {
    if (nullptr == singleton_) {
        pthread_mutex_lock(&singleton_mutex_);
        if (nullptr == singleton_) {
            singleton_ = new Redis();
        }
        pthread_mutex_unlock(&singleton_mutex_);
    }
    return singleton_;
}


/**
 * 获取一个redis连接，成功则返回true。
 * @param conn
 * @return
 */
bool Redis::GetConnection(redisContext **conn) {
    bool get_flag = false;
    pthread_mutex_lock(&pool_mutex_);
    if (!pool_.empty()) {
        *conn = pool_[0];
        pool_.erase(pool_.begin(), pool_.begin() + 1);
        get_flag = true;
    }
    pthread_mutex_unlock(&pool_mutex_);
    return get_flag;
}

/**
 * 释放一个连接，成功返回true。
 * @param conn
 * @return
 */
bool Redis::ReleaseConnection(redisContext *conn) {
    if (nullptr == conn) {
        return false;
    }
    pthread_mutex_lock(&pool_mutex_);
    pool_.emplace_back(conn);
    pthread_mutex_unlock(&pool_mutex_);
    return true;
}

/**
 * 判断目标的ip地址是否有对应的session记录，如果有就返回session。没有就新建一个session并返回。
 * @param conn
 * @param ip
 * @return
 */
std::string Redis::SessionExists(redisContext *context, const std::string &ip) {
    pthread_mutex_lock(&connection_mutex_);  // 即便是取出了连接，那也是一样需要上锁的，因为操作的数据库是同一个。
    redisReply *reply = (redisReply *) redisCommand(context, "HGET connection %s", ip.c_str());
    if (reply != nullptr && reply->str != nullptr) {
        std::string value = reply->str;
        freeReplyObject(reply);
        pthread_mutex_unlock(&connection_mutex_);  // 操作结束，解锁。
        return value;
    } else {
        freeReplyObject(reply);
        std::string result = Utils::GenerationSession(
                Config::get_singleton_()->redis_generator_session_length_);  // 生成固定位数的session字符串。
        redisCommand(context, "HSET connection %s %s", ip.c_str(), result.c_str());
        pthread_mutex_unlock(&connection_mutex_);  // 操作结束，解锁。
        return result;
    }
}
