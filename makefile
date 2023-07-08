CC = g++
Flag = -g -c -std=c++11

all: FileWebServer

Config/Config.o: Config/Config.cpp
	$(CC) $(Flag) $^ -o $@

HTTPConnection/HTTPConnection.o: HTTPConnection/HTTPConnection.cpp
	$(CC) $(Flag) $^ -o $@

Log/Log.o: Log/Log.cpp
	$(CC) $(Flag) $^ -o $@

MysqlConnectionPool/MysqlConnectionPool.o: MysqlConnectionPool/MysqlConnectionPool.cpp
	$(CC) $(Flag) $^ -o $@

Redis/Redis.o: Redis/Redis.cpp
	$(CC) $(Flag) $^ -o $@

ThreadPool/ThreadPool.o: ThreadPool/ThreadPool.cpp
	$(CC) $(Flag) $^ -o $@

timer/timer.o: timer/timer.cpp
	$(CC) $(Flag) $^ -o $@

Utils/Utils.o: Utils/Utils.cpp
	$(CC) $(Flag) $^ -o $@

WebServer/WebServer.o: WebServer/WebServer.cpp
	$(CC) $(Flag) $^ -o $@

main.o: main.cpp
	$(CC) $(Flag) $^ -o $@

FileWebServer: Config/Config.o HTTPConnection/HTTPConnection.o Log/Log.o MysqlConnectionPool/MysqlConnectionPool.o Redis/Redis.o ThreadPool/ThreadPool.o timer/timer.o Utils/Utils.o WebServer/WebServer.o main.o
	$(CC) -g $^ -o $@ -lpthread -lmysqlclient -lhiredis

.PHONY:clean
clean:
	rm Config/Config.o HTTPConnection/HTTPConnection.o Log/Log.o MysqlConnectionPool/MysqlConnectionPool.o Redis/Redis.o ThreadPool/ThreadPool.o timer/timer.o Utils/Utils.o WebServer/WebServer.o main.o FileWebServer
	rm `find LogDir/*`
	rm `find ./core*`