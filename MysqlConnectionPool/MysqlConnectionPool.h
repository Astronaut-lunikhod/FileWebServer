//
//  Created by diode on 23-6-25.
//  数据库连接池
//

#ifndef CHATROOM_MYSQLCONNECTIONPOOL_H
#define CHATROOM_MYSQLCONNECTIONPOOL_H

#include <iostream>
#include <mysql/mysql.h>
#include <pthread.h>
#include <string>
#include <cstring>
#include "../Config/Config.h"
#include <vector>
#include <assert.h>

class MysqlConnectionPool {
private:
    std::string host_;  // 数据库连接五要素
    std::string port_;
    std::string username_;
    std::string password_;
    std::string database_name_;
    unsigned connection_max_num_;  // 连接池中最大的连接数量。
    std::vector<MYSQL *> mysql_pool_;  // 连接池
    pthread_mutex_t mysql_pool_mutex_;  // 操作连接池的互斥锁。
    unsigned used_connection_num_;  // 正在使用的连接数量。

    static MysqlConnectionPool *mysql_connection_pool_singleton_instance_;  // 数据库连接池的单例对象以及对应的静态互斥锁。

    static pthread_mutex_t singleton_instance_mutex_;
public:

private:
    MysqlConnectionPool();

    ~MysqlConnectionPool();

    bool InitTable();  // 初始化数据库的表。
public:
    static MysqlConnectionPool *get_mysql_connection_pool_singleton_instance_();

    bool GetConnection(MYSQL **conn);  // 随机获取一个空闲状态的数据库连接，从连接池中取走。

    bool ReleaseConnection(MYSQL *conn);  // 释放一个连接，归还到连接池中。

    bool Login(int &user_id, const std::string &username, const std::string &password);  // 登录，查看用户名密码是否正确。

    bool Register(const std::string &username, const std::string &password);  // 注册，查看用户名是否重名，如果没有重名，直接插入，都成功则返回true。
};

#endif
