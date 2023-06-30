#include "Config/Config.h"
#include "ThreadPool/ThreadPool.h"
#include "WebServer/WebServer.h"
#include "MysqlConnectionPool/MysqlConnectionPool.h"

int main() {
    Config::get_singleton_();
    MysqlConnectionPool::get_mysql_connection_pool_singleton_instance_();
    ThreadPool::get_singleton_();
    WebServer webServer;
    webServer.InitialConnection();
    webServer.EpollLoop();
    return 0;
}
