#include <iostream>
#include "Config/Config.h"
#include "WebServer/WebServer.h"
#include "ThreadPool/ThreadPool.h"

int main() {
    Config::GetSingleton_();
    ThreadPool::GetSingleton();
    WebServer webServer;
    webServer.EventListen();
    webServer.EpollLoop();
    return 0;
}
