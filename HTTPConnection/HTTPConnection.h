//
// Created by diode on 23-6-29.
// 连接的实体类，对应每一个连接，对其所有的操作也包含其中。
//

#ifndef FILEWEBSERVER_HTTPCONNECTION_H
#define FILEWEBSERVER_HTTPCONNECTION_H

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <cerrno>
#include <cstdarg>
#include <fstream>
#include "../WebServer/WebServer.h"
#include "../MysqlConnectionPool/MysqlConnectionPool.h"

class HTTPConnection {
private:
    enum METHOD {
        GET = 0,
        POST
    };

    enum MAIN_MACHINE_STATE {  // 主状态机的所有状态。
        LINE = 0,  // 当前需要解析行。
        HEAD,  // 头
        BODY,  // 体
        OVER  // 已经解析完毕。
    };

    enum SUB_MACHINE_STATE {  // 从状态机的所有状态。
        GOOD_LINE = 0,  // 当前行截断以后的结果是好的。
        CONTINUE_LINE,  // 当前行截断以后的结果是需要继续接收报文。
        BAD_LINE  // 当前行存在语法错误。
    };

    enum REQUEST_CODE {  // 解析到目前，HTTP的请求报文解析状态。
        GOOD_REQUEST = 0,  // 完全解析结束，报文是好的。
        TEMP_GOOD_REQUEST,  // 就当前行的结果来看，暂时是好的,但是还需要继续解析。
        BAD_REQUEST  // 当前行存在问题，一定是坏的。
    };

    enum RESPONSE_CODE {  // 返回的页面的状态码。
        DISK_HTML = 0,  // 特殊情况，代表返回的DISK请求。
        DOWNLOAD = 1,  // 用户想要下载文件。
        DELETE = 2,  // 用户想要删除文件。
        OK = 200, // 一切OK，准备回送。
        BAD_RESPONSE = 400,  // 解析的过程中存在错误。
        FORBIDDEN = 403,  // 禁止访问。
        NOT_FOUND = 404  // 内容未找到。
    };

    sockaddr_in client_address_;

    unsigned read_buffer_max_len_;  // read_buffer_的大小。
    char *read_buffer_;
    unsigned read_next_idx_;  // read的下一个位置。
    unsigned main_next_idx_;  // main解析的下一个位置。
    unsigned sub_next_idx_;  // sub截断的下一个位置。

    SUB_MACHINE_STATE sub_machine_state_;
    MAIN_MACHINE_STATE main_machine_state_;
    METHOD method_;
    bool new_url_;  // 请求报文中的url，这个bool代表是否new了url_。
    char *url_;
    bool new_version_;
    char *version_;
    bool linger_;
    unsigned body_length_;
    bool new_host_;
    char *host_;
    bool new_body_;
    char *body_;
    unsigned response_file_path_max_len_;  // 该响应页面的最大长度。
    char *response_file_path_;  // 响应的HTML页面的完全路径。
    char *map_response_file_address_;  // 将页面加载到内存中去。
    struct stat response_file_stat_;  // 响应文件的详情。
    unsigned write_buffer_max_len_;
    char *write_buffer_;
    unsigned write_buffer_next_idx_;
    bool login_state_;  // 登录状态，用于进入磁盘页面。
    bool new_js_;  // 是否new了js。
    char *js_;

    int user_id_;  // 用户的id。
    std::string username_;  // 用户名。
    std::string user_file_root_path_;  // 当前用户的根目录,这是不可以改变的。
    std::string pwd_;  // 用户当前站立的位置（默认是根目录），这里是可以移动改变的。
    std::string boundary_;  // 上传文件的时候使用的分界符。
    int blank_count_ = 0;  // 记录空行出现次数，因为上传文件会有两次。
    std::string upload_file_name_;  // 上传的文件的名字,回溯HTTP状态的时候也需要clear()。
    std::string download_file_name_;  // 用户本次想要下载的文件的名字，回溯的时候也需要clear()。
    std::string delete_file_name_;  // 删除文件的名字，回溯的时候需要clear()。
    std::string share_file_name_;  // 分享或者取消分享的时候使用的文件名，回溯的时候需要clear()。
    std::string entry_dir_name_;  // 进入的文件夹的名字，回溯的时候需要clear()。

public:
    enum WORK_MODE {  // 当前对象接下来应该做什么工作，是读，处理逻辑还是写
        READ = 0,
        PROCESS,
        WRITE
    };

    WORK_MODE work_mode_;
    int client_fd_;
    int epoll_fd_;
    bool level_trigger_;  // 是否是水平触发的。
    bool one_shot_;  // 是否使用one_shot。

    struct iovec iov_[3];
    int iov_count_;
    unsigned byte_to_send;  // 等待发送的。
    unsigned byte_have_send;  // 已经发送的。
private:
    SUB_MACHINE_STATE CutLine();  // 移动从状态机的读标头，找出对应的部分。

    REQUEST_CODE AnalysisLine(std::string line);  // 解析行。

    REQUEST_CODE AnalysisHead(std::string line);  // 解析头。

    REQUEST_CODE AnalysisBody(std::string line);  // 解析请求体。

    RESPONSE_CODE ConstructorHtml();  // 当分析请求的结果是GOOD的时候，可以根据请求的内容构建HTML页面，并且记得返回构建页面对应的response_code。

    bool ConstructResponse(RESPONSE_CODE response_code);  // 根据构造好的HTML，创建对应的数据包。请求头来源于response_code。

    bool AddResponse(const char *format, ...);  // 往buffer缓冲中写内容。

    bool AddLine(int response_code, const char *response_phrase);  // 写入line。

    bool AddHeader(int content_length, const std::string &content_type);  // 新增响应头

    bool AddBody(const char *body);  // 增加响应体。

    void UpdateIov(int tmp, char *starts[]);  // 根据发送的内容，修改iov，因为iov不仅仅只有一个或者两个了，现在可以多个。
public:
    HTTPConnection();

    ~HTTPConnection();

    void Establish(int client_fd, int epoll_fd,
                   const sockaddr_in &client_address);  // 当建立了一个连接一个都需要使用这个函数进行初始化。Init是在完成了一个连接的所有操作之后，进行状态的回调。

    void Init();  // 进行状态回调，回到刚建立连接的时候。

    bool ReceiveMessage();  // 读取信息。

    bool Process();  // 对读取的内容进行处理解析，并构造好需要返回的内容，如果中间出现错误，直接返回false，将会直接关闭连接。

    REQUEST_CODE AnalysisRequestMessage();  // 解析request报文，并返回对应的代码。

    bool WriteHTTPMessage();  // 发送缓冲区和请求体中的内容。
};

#endif