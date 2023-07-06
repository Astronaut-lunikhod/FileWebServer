#include "Config/Config.h"
#include "ThreadPool/ThreadPool.h"
#include "WebServer/WebServer.h"
#include "MysqlConnectionPool/MysqlConnectionPool.h"
#include "Redis/Redis.h"
#include "Log/Log.h"

int main() {
    Config::get_singleton_();
    Redis::get_singleton_();
    Log::get_log_singleton_instance_();
    MysqlConnectionPool::get_mysql_connection_pool_singleton_instance_();
    ThreadPool::get_singleton_();
    WebServer webServer;
    webServer.InitialConnection();
    webServer.EpollLoop();
    return 0;
}
