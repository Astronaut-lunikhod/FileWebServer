//
//  Created by diode on 23-6-25.
// 数据库连接池的实现类
//

#include "MysqlConnectionPool.h"

MysqlConnectionPool *MysqlConnectionPool::mysql_connection_pool_singleton_instance_ = nullptr;  // 数据库连接池的单例对象以及对应的静态互斥锁。
pthread_mutex_t MysqlConnectionPool::singleton_instance_mutex_ = PTHREAD_MUTEX_INITIALIZER;

// 完成所有的初始化工作。
MysqlConnectionPool::MysqlConnectionPool() {  // 构造函数，通过Config的单例模型获取一些必要参数
    host_ = Config::get_singleton_()->mysql_host_;
    port_ = Config::get_singleton_()->mysql_port_;
    username_ = Config::get_singleton_()->mysql_username_;
    password_ = Config::get_singleton_()->mysql_password_;
    database_name_ = Config::get_singleton_()->mysql_database_name_;
    connection_max_num_ = Config::get_singleton_()->mysql_connection_max_num_;
    used_connection_num_ = 0;
    pthread_mutex_init(&mysql_pool_mutex_, nullptr);
    for (int i = 0; i < this->connection_max_num_; ++i) {
        MYSQL *conn = nullptr;
        conn = mysql_init(conn);
        assert(conn != nullptr);  // 如果为空，那就说明初始化存在问题。
        mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8");  // 设置字符集
        conn = mysql_real_connect(conn, host_.c_str(), username_.c_str(),
                                  password_.c_str(),
                                  database_name_.c_str(), stoi(port_),
                                  nullptr, 0);  // 与数据库建立连接
        assert(conn != nullptr);
        this->mysql_pool_.emplace_back(conn);
    }
    InitTable();  // 开始初始化表
}


/**
 * 释放所有的空闲连接。
 */
MysqlConnectionPool::~MysqlConnectionPool() {
    pthread_mutex_lock(&mysql_pool_mutex_);
    if (!mysql_pool_.empty()) {
        for (std::vector<MYSQL *>::iterator ite = mysql_pool_.begin(); ite != mysql_pool_.end(); ++ite) {
            MYSQL *conn = *ite;  // 这里是因为使用的迭代器，所以需要解引用，如果是增强for循环则不需要。
            mysql_close(conn);
        }
    }
    pthread_mutex_unlock(&mysql_pool_mutex_);
    pthread_mutex_destroy(&singleton_instance_mutex_);
}


bool MysqlConnectionPool::InitTable() {  // 初始化所有可能会使用到的表。
    MYSQL *conn = nullptr;
    if (GetConnection(&conn)) {  // 一开始的时候一定是空闲状态，所以可以初始化表。
        mysql_query(conn, "create table if not exists user(userid int primary key auto_increment, username varchar(128),password varchar(128));");
        assert(mysql_errno(conn) == 0);  // 这时候是没有发生错误的
    }
    ReleaseConnection(conn);  // 使用结束请将资源释放掉。
    return true;
}


MysqlConnectionPool *MysqlConnectionPool::get_mysql_connection_pool_singleton_instance_() {  // 懒汉式构造单例
    if (nullptr == mysql_connection_pool_singleton_instance_) {
        pthread_mutex_lock(&singleton_instance_mutex_);
        if (nullptr == mysql_connection_pool_singleton_instance_) {
            mysql_connection_pool_singleton_instance_ = new MysqlConnectionPool();
        }
        pthread_mutex_unlock(&singleton_instance_mutex_);
    }
    return mysql_connection_pool_singleton_instance_;
}


/**
 * 获取一个空闲连接，如果成功返回true，否则返回false。
 * @param conn
 * @return
 */
bool MysqlConnectionPool::GetConnection(MYSQL **conn) {
    bool get_flag = false;
    pthread_mutex_lock(&mysql_pool_mutex_);
    if (!mysql_pool_.empty()) {
        *conn = mysql_pool_[0];
        mysql_pool_.erase(mysql_pool_.begin(), mysql_pool_.begin() + 1);
        get_flag = true;  // 如果有空闲连接，那么就是true，否则就是false。
        ++used_connection_num_;
    }
    pthread_mutex_unlock(&mysql_pool_mutex_);
    return get_flag;
}

/**
 * 归还一个连接给连接池。
 * @param conn
 * @return
 */
bool MysqlConnectionPool::ReleaseConnection(MYSQL *conn) {
    if (nullptr == conn) {  // 因为是空连接，所以释放失败。
        return false;
    }
    pthread_mutex_lock(&mysql_pool_mutex_);
    mysql_pool_.emplace_back(conn);
    --used_connection_num_;
    pthread_mutex_unlock(&mysql_pool_mutex_);
    return true;
}


/**
 * 登录，查看用户名密码是否正确。
 * @param username
 * @param password
 * @return
 */
bool MysqlConnectionPool::Login(int &user_id, const std::string &username, const std::string &password) {
    bool ret = false;
    MYSQL *conn = nullptr;
    if (GetConnection(&conn)) {  // 一开始的时候一定是空闲状态，所以可以初始化表。
        pthread_mutex_lock(&mysql_pool_mutex_);
        char *sql = new char[256]{'\0'};
        snprintf(sql, 256, "select * from user where username = '%s' and password = '%s';", username.c_str(),
                 password.c_str());
        mysql_query(conn, sql);
        MYSQL_RES *res = mysql_store_result(conn);
        ret = mysql_num_rows(res) > 0;  // 判断查询结果是否存在，存在就代表登录成功。
        while(MYSQL_ROW row = mysql_fetch_row(res)) {
            user_id = atoi(row[0]);
        }
        mysql_free_result(res);  // 及时的释放结果。
        assert(mysql_errno(conn) == 0);  // 这时候是没有发生错误的
        pthread_mutex_unlock(&mysql_pool_mutex_);  // 操作连接池一定要锁定，不然就会出现线程不安全的问题。
        delete[] sql;
    }
    ReleaseConnection(conn);  // 使用结束请将资源释放掉。
    return ret;
}


/**
 * 注册，查看用户名是否重名，如果没有重名，直接插入，都成功则返回true。
 */
bool MysqlConnectionPool::Register(const std::string &username, const std::string &password) {
    bool ret = false;
    MYSQL *conn = nullptr;
    if (GetConnection(&conn)) {  // 一开始的时候一定是空闲状态，所以可以初始化表。
        pthread_mutex_lock(&mysql_pool_mutex_);  // 操作连接池一定要锁定，不然就会出现线程不安全的问题。
        char *sql = new char[256]{'\0'};
        snprintf(sql, 256, "select * from user where username = '%s';", username.c_str());
        mysql_query(conn, sql);
        MYSQL_RES *res = mysql_store_result(conn);
        ret = mysql_num_rows(res) == 0;  // 判断查询结果是否为空，如果不为空，那就说明存在重名。
        mysql_free_result(res);  // 及时的释放结果。
        if (!ret) {
            pthread_mutex_unlock(&mysql_pool_mutex_);
            delete[] sql;
            ReleaseConnection(conn);  // 使用结束请将资源释放掉。
            return false;
        }
        snprintf(sql, 256, "insert into user(username, password) values('%s', '%s');", username.c_str(), password.c_str());
        mysql_query(conn, sql);
        res = mysql_store_result(conn);
        ret = mysql_affected_rows(conn) == 1;
        mysql_free_result(res);  // 及时的释放结果。
        if (!ret) {
            pthread_mutex_unlock(&mysql_pool_mutex_);
            delete[] sql;
            ReleaseConnection(conn);  // 使用结束请将资源释放掉。
            return false;
        }
        assert(mysql_errno(conn) == 0);  // 这时候是没有发生错误的
        pthread_mutex_unlock(&mysql_pool_mutex_);
        delete[] sql;
    }
    ReleaseConnection(conn);  // 使用结束请将资源释放掉。
    return ret;
}