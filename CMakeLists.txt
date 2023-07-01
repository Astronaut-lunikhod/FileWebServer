cmake_minimum_required(VERSION 3.25)
project(FileWebServer)

set(CMAKE_CXX_STANDARD 11)

add_executable(FileWebServer main.cpp Config/Config.h Config/Config.cpp HTTPConnection/HTTPConnection.h HTTPConnection/HTTPConnection.cpp WebServer/WebServer.h WebServer/WebServer.cpp Utils/Utils.h ThreadPool/ThreadPool.h ThreadPool/ThreadPool.cpp MysqlConnectionPool/MysqlConnectionPool.cpp)

target_link_libraries(FileWebServer pthread mysqlclient)