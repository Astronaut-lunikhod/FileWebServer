//
// Created by diode on 23-6-29.
// 连接的实体类，对应每一个连接，对其所有的操作也包含其中。
//

#include "HTTPConnection.h"


/**
 * 移动从状态机的读标头，找出对应的部分。
 * @return
 */
HTTPConnection::SUB_MACHINE_STATE HTTPConnection::CutLine() {
    if (read_next_idx_ < body_length_) {  // 说明字节数量严重不足，需要重新进行接收。
        return SUB_MACHINE_STATE::CONTINUE_LINE;
    }
    if (blank_count_ == 2) {  // 如果是上传文件的时候，那么请求体是不需要将结尾的\r\n换乘结束符号的，后面可以自己计算。
        sub_next_idx_ = read_next_idx_;
        return SUB_MACHINE_STATE::GOOD_LINE;
    }
    char tmp;
    for (; sub_next_idx_ < read_next_idx_; ++sub_next_idx_) {
        tmp = read_buffer_[sub_next_idx_];
        if (tmp == '\r') {
            if (sub_next_idx_ + 1 >= read_next_idx_) {  // 解析接收内容，为了找到截断符号。
                return SUB_MACHINE_STATE::CONTINUE_LINE;
            } else if (read_buffer_[sub_next_idx_ + 1] == '\n') {  // 找到了截断符号
                read_buffer_[sub_next_idx_++] = '\0';
                read_buffer_[sub_next_idx_++] = '\0';
                return SUB_MACHINE_STATE::GOOD_LINE;
            } else {
                return SUB_MACHINE_STATE::BAD_LINE;
            }
        } else if (tmp == '\n') {
            if (sub_next_idx_ > 1 && read_buffer_[sub_next_idx_ - 1] == '\r') {  // 可能是之前继续接收了，所以要连起来看。
                read_buffer_[sub_next_idx_ - 1] = '\0';
                read_buffer_[sub_next_idx_++] = '\0';
                return SUB_MACHINE_STATE::GOOD_LINE;
            }
            if (boundary_.empty())  // 只有不是文件上传模式的时候才有用。
                return SUB_MACHINE_STATE::BAD_LINE;
        }
    }
    if (method_ == METHOD::POST && sub_next_idx_ - main_next_idx_ == body_length_)
        return SUB_MACHINE_STATE::GOOD_LINE;
    return SUB_MACHINE_STATE::CONTINUE_LINE;  // 没有找到结束符号，那就代表是需要继续接收数据包。
}


/**
 * 解析行。
 * @return
 */
HTTPConnection::REQUEST_CODE HTTPConnection::AnalysisLine(std::string line) {
    size_t found = line.find_first_of(" \t");
    if (found == std::string::npos) {  // 解析失败，找不到分割符号。
        return REQUEST_CODE::BAD_REQUEST;
    }
    std::string method = line.substr(0, found);
    if (method == "GET") {
        method_ = METHOD::GET;
    } else if (method == "POST") {
        method_ = METHOD::POST;
    } else {
        return REQUEST_CODE::BAD_REQUEST;  // 解析失败，超过了预定的可解析方法。
    }
    line = line.substr(found + 1);  // 删除开头部分。

    found = line.find_first_of(" \t");
    if (found == std::string::npos) {
        return REQUEST_CODE::BAD_REQUEST;  // 解析失败，找不到分割符号。
    }
    url_ = new char[found + 1]{'\0'};
    new_url_ = true;
    strcpy(url_, Utils::UrlDecode(line.substr(0, found)).c_str());
    line = line.substr(found + 1);

    found = line.find_first_of(" \t");
    if (found != std::string::npos) {
        return REQUEST_CODE::BAD_REQUEST;  // 解析失败，找不到分割符号。
    }
    version_ = new char[line.size() + 1]{'0'};
    new_version_ = true;
    strcpy(version_, line.substr(0, line.size()).c_str());
    if (strcmp(version_, "HTTP/1.1") == 0) {
        main_machine_state_ = MAIN_MACHINE_STATE::HEAD;
        return REQUEST_CODE::TEMP_GOOD_REQUEST;  // 一行都解析成功了，但是还需要继续解析。
    } else {
        return REQUEST_CODE::BAD_REQUEST;  // 解析失败，预料之外的内容。
    }
}

/**
 * 解析头。
 * @param line
 * @return
 */
HTTPConnection::REQUEST_CODE HTTPConnection::AnalysisHead(std::string line) {
    if (line.find(boundary_) != std::string::npos &&
        !boundary_.empty()) {  // 如果上传文件的请求，这时代表已经进入了数据体的部分，需要开始计算有哪些部分是需要减去的。
        start_boundary_ = true;
    }
    if (start_boundary_) {
        mines_count += line.size() + 2;
    }
    if (line[0] == '\0') {
        blank_count_++;
        if (blank_count_ == 1 && boundary_.size() >= 2) {  // 说明这是上传文件的时候遇见的第一个空行，也就是头还没有解析结束。
            return REQUEST_CODE::TEMP_GOOD_REQUEST;
        }
        if (method_ == METHOD::POST) {  // 如果是POST请求，那就需要读取请求体。
            main_machine_state_ = MAIN_MACHINE_STATE::BODY;
            return REQUEST_CODE::TEMP_GOOD_REQUEST;
        } else if (method_ == METHOD::GET) {  // 如果是GET请求，那就已经结束了。
            main_machine_state_ = MAIN_MACHINE_STATE::OVER;
            return REQUEST_CODE::GOOD_REQUEST;
        }
    } else if (line.compare(0, 11, "Connection:") == 0) {
        size_t found = line.find_first_of(" \t");
        if (found == 11) {
            line = line.substr(found + 1);
            if (line == "keep-alive") {
                linger_ = true;
            }
        }
    } else if (line.compare(0, 15, "Content-Length:") == 0 || line.compare(0, 15, "Content-length:") == 0) {
        size_t found = line.find_first_of(" \t");
        if (found == 15) {
            line = line.substr(found + 1);
            body_length_ = std::stoi(line);
        }
    } else if (line.compare(0, 5, "Host:") == 0) {
        size_t found = line.find_first_of(" \t");
        if (found == 5) {
            line = line.substr(found + 1);
            host_ = new char[line.size() + 1]{'\0'};
            new_host_ = true;
            strcpy(host_, line.c_str());
        }
    } else if (line.compare(0, 13, "Content-Type:") == 0) {
        size_t found = line.find_first_of(" \t");
        if (found == 13) {
            line = line.substr(found + 1);
            if (line.compare(0, 20, "multipart/form-data;") == 0) {  // 是上传文件的，否则并不重要的。
                found = line.find("boundary=") + 9;
                boundary_ = line.substr(found);
            }
        }
    } else if (line.find("filename=") != std::string::npos) {  // 如果存在文件的名字。
        size_t found = line.find("filename=");
        line = line.substr(found + 9 + 1);  // +9是因为filename=,加1是为了双引号
        upload_file_name_ = line.substr(0, line.size() - 1);  // 去掉结尾的双引号。
    }
    return REQUEST_CODE::TEMP_GOOD_REQUEST;  // 解析没有报错，可以继续下一行。
}


/**
 * 解析请求体。
 * @param line
 * @return
 */
HTTPConnection::REQUEST_CODE HTTPConnection::AnalysisBody(std::string line) {
    if (start_boundary_) {  // 同样，真实的数据的尾部还带有一些冗余的信息，需要删除。删完以后，关闭删除计数。等待Init回溯状态。
        mines_count = mines_count + 2 + 2 + boundary_.size() + 2 + 2;
        start_boundary_ = false;
    }
    if (!boundary_.empty() && blank_count_ == 2) {  // 上传文件需要特殊处理,这个时候千万不能保存，因为要等到确认了身份权限才可以保存。
        body_length_ = body_length_ - mines_count;  // 计算真正的数据体有多长。
        body_ = new char[body_length_ + 1]{'\0'};
        new_body_ = true;
        mempcpy(body_, read_buffer_ + main_next_idx_, body_length_);  // 复制数据体内容到body中，而且有终止符也不会停下来，一定复制n个元素。
        main_machine_state_ = MAIN_MACHINE_STATE::OVER;
        return REQUEST_CODE::GOOD_REQUEST;
    }
    if (line.size() == body_length_) {
        line[body_length_] = '\0';
        body_ = new char[line.size() + 1]{'\0'};
        new_body_ = true;
        strcpy(body_, line.c_str());
        main_machine_state_ = MAIN_MACHINE_STATE::OVER;
        return REQUEST_CODE::GOOD_REQUEST;
    }
    return REQUEST_CODE::TEMP_GOOD_REQUEST;  // 因为请求体没有全部接收到，所以需要重新接收。
}

/**
 * 当分析请求的结果是GOOD的时候，可以根据请求的内容构建HTML页面，并且记得返回构建页面对应的response_code。
 * @return
 */
HTTPConnection::RESPONSE_CODE HTTPConnection::ConstructorHtml() {
    int len = strlen(response_file_path_);
    RESPONSE_CODE ret = RESPONSE_CODE::NOT_FOUND;
    const char *p = strchr(url_, '/');  // 查询url地址最右边的/，目的是为了找出请求目标
    std::string htmls[]{"/register.html", "/register_error.html", "/login.html", "/login_error.html"};
    for (int i = 0; i < 4; ++i) {  // 遍历看看是不是不需要权限就可以访问的基础页面。
        if (strcmp(p, htmls[i].c_str()) == 0) {  // 确实在基础页面之内。
            strncpy(response_file_path_ + len, htmls[i].c_str(), htmls[i].size());
            ret = RESPONSE_CODE::OK;
            break;
        }
    }
    std::string request = "/login";
    if (strncmp(p, request.c_str(), strlen(request.c_str())) == 0 &&
        method_ == POST) {  // 如果是/login请求，切割原始的body_，找出账号密码。
        char *tmp = new char[body_length_ + 1]{'\0'};
        strcpy(tmp, body_);
        char *split = strchr(tmp, '&');
        split[0] = '\0';
        std::string username = tmp, password = split + 1;
        int found = username.find('=');
        username = username.substr(found + 1);
        found = password.find('=');
        password = password.substr(found + 1);
        if (MysqlConnectionPool::get_mysql_connection_pool_singleton_instance_()->Login(user_id_, username,
                                                                                        password)) {  // 根据数据库查询结果判断跳转。
            login_state_ = true;  // 用于判断进入磁盘页面。
            username_ = username;
            user_file_root_path_ =
                    Config::get_singleton_()->http_connection_file_dir_root_path_ + "/" + username;  // 用户的网盘根路径。
            pwd_ = user_file_root_path_;
            strncpy(response_file_path_ + len, "/disk.html\0", strlen("/disk.html\0"));
            ret = RESPONSE_CODE::DISK_HTML;
        } else {
            strncpy(response_file_path_ + len, "/login_error.html\0", strlen("/login_error.html\0"));
            ret = RESPONSE_CODE::OK;
        }
    }
    request = "/register";
    if (strncmp(p, request.c_str(), strlen(request.c_str())) == 0 &&
        method_ == POST) {  // 如果是/register请求，切割原始的body_，找出账号密码。
        char *tmp = new char[body_length_ + 1]{'\0'};
        strcpy(tmp, body_);
        char *split = strchr(tmp, '&');
        split[0] = '\0';
        std::string username = tmp, password = split + 1;
        int found = username.find('=');
        username = username.substr(found + 1);
        found = password.find('=');
        password = password.substr(found + 1);
        if (MysqlConnectionPool::get_mysql_connection_pool_singleton_instance_()->Register(username, password)) {
            assert(mkdir((Config::get_singleton_()->http_connection_file_dir_root_path_ + "/" + username).c_str(),
                         S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH |
                         S_IWOTH | S_IXOTH) == 0);  // 创建目标用户的文件夹。
            strncpy(response_file_path_ + len, "/login.html\0", strlen("/login.html\0"));
        } else {
            strncpy(response_file_path_ + len, "/register_error.html\0", strlen("/register_error.html\0"));
        }
        ret = RESPONSE_CODE::OK;
    }
    request = "/disk.html";  // 直接访问disk.html，而不是我response返回的。
    if (strncmp(p, request.c_str(), strlen(request.c_str())) == 0) {  // 如果是/disk.html，判断是否已经登录了。。
        if (login_state_) {  // 登录了就可以。
            strncpy(response_file_path_ + len, "/disk.html\0", strlen("/disk.html\0"));
            ret = RESPONSE_CODE::DISK_HTML;
        } else {  // 没有权限访问。
            strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
            ret = RESPONSE_CODE::FORBIDDEN;
        }
    }
    request = "/upload";  // 上传文件
    if (strncmp(p, request.c_str(), strlen(request.c_str())) == 0) {  // 如果是/upload请求，判断是否已经登录了。。
        if (login_state_ &&
            (Utils::InDir(user_file_root_path_, pwd_) || user_file_root_path_ == pwd_)) {  // 登录,并且当前的位置处于目标自己的根目录之下。
            strncpy(response_file_path_ + len, "/disk.html\0", strlen("/disk.html\0"));
            // 将body保存文件
            std::ofstream fout(pwd_ + "/" + upload_file_name_, std::ios::out);
            fout.write(body_, body_length_);
            fout.flush();  // 刷新缓冲。
            fout.close();
            chmod((pwd_ + "/" + upload_file_name_).c_str(),
                  S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH |
                  S_IWOTH);  // 设置文件的权限,有S_IXOTH代表分享了，否则就是没有分享了。
            ret = RESPONSE_CODE::DISK_HTML;
        } else {  // 没有权限访问。
            strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
            ret = RESPONSE_CODE::FORBIDDEN;
        }
    }
    request = "/download";
    if (strncmp(p, request.c_str(), strlen(request.c_str())) == 0) {  // 如果是/download请求，判断是否已经登录了。
        if (login_state_ &&
            (Utils::InDir(user_file_root_path_, pwd_) || user_file_root_path_ == pwd_)) {  // 登录,并且当前的位置处于目标自己的根目录之下。
            const char *pos = strrchr(url_, '=');  // 找到等号的位置，操作的文件就在等号的右边。
            download_file_name_ = pos + 1;
            if (download_file_name_.find_first_of('/') != std::string::npos ||
                download_file_name_.find_first_of('\\') != std::string::npos) {  // 说明存在恶意调用,限制操作级别只能是一个级别之内。
                strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
                ret = RESPONSE_CODE::FORBIDDEN;
            } else {
                return RESPONSE_CODE::DOWNLOAD;  // 不再加载，因为在构造响应体的时候会加载文件。
            }
        } else {  // 没有权限访问。
            strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
            ret = RESPONSE_CODE::FORBIDDEN;
        }
    }
    request = "/preview";
    if (strncmp(p, request.c_str(), strlen(request.c_str())) == 0) {  // 如果是/preview，需要登录，因为这是系统要用的资源文件。
        size_t equal_position = strrchr(url_, '=') - url_;  // 找到等号的位置，操作的文件就在等号的右边。
        size_t split_position = request.find_last_of('/');  // 找到分割符号的位置。
        if (split_position != 0) {  // 说明想要分级，这有可能是人为的，不正确。
            return RESPONSE_CODE::FORBIDDEN;
        }
        resource_file_name_ = url_ + equal_position + 1;
        std::string final_path = pwd_ + "/" + resource_file_name_;
        strncpy(response_file_path_, final_path.c_str(), final_path.size());
        ret = RESPONSE_CODE::RESOURCE_HTML;
    }
    request = "/download2copy";
    if (strncmp(p, request.c_str(), strlen(request.c_str())) == 0) {  // 如果是/download2copy请求，判断是否已经登录了,将已经分享的文件下载到本地。
        if (login_state_ &&
            (Utils::InDir(Config::get_singleton_()->http_connection_file_dir_root_path_, pwd_) ||
             Config::get_singleton_()->http_connection_file_dir_root_path_ == pwd_)) {  // 登录,当前位置必须要终极根目录下。
            const char *pos = strrchr(url_, '=');  // 找到等号的位置，操作的文件就在等号的右边。
            download_file_name_ = pos + 1;
            if (download_file_name_.find_first_of('/') != std::string::npos ||
                download_file_name_.find_first_of('\\') != std::string::npos) {  // 说明存在恶意调用,限制操作级别只能是一个级别之内。
                strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
                ret = RESPONSE_CODE::FORBIDDEN;
            } else {
                // 判断目标文件是否可以下载
                std::string final_path = pwd_ + "/" + download_file_name_;
                struct stat file_stat;
                assert(stat(final_path.c_str(), &file_stat) != -1);  // 取出权限。
                if (!((file_stat.st_mode & (0777)) & S_IXOTH)) {  // 其他人不可执行，代表不是可分享的文件，无法下载。
                    strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
                    ret = RESPONSE_CODE::FORBIDDEN;
                } else {
                    return RESPONSE_CODE::DOWNLOAD;  // 不再加载，因为在构造响应体的时候会加载文件。
                }
            }
        } else {  // 没有权限访问。
            strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
            ret = RESPONSE_CODE::FORBIDDEN;
        }
    }
    request = "/copy";
    if (strncmp(p, request.c_str(), strlen(request.c_str())) == 0) {  // 如果是/download2copy请求，判断是否已经登录了,将已经分享的文件下载到本地。
        if (login_state_ &&
            (Utils::InDir(Config::get_singleton_()->http_connection_file_dir_root_path_, pwd_) ||
             Config::get_singleton_()->http_connection_file_dir_root_path_ == pwd_)) {  // 登录,当前位置必须要终极根目录下。
            const char *pos = strrchr(url_, '=');  // 找到等号的位置，操作的文件就在等号的右边。
            copy_file_name_ = pos + 1;
            if (copy_file_name_.find_first_of('/') != std::string::npos ||
                copy_file_name_.find_first_of('\\') != std::string::npos) {  // 说明存在恶意调用,限制操作级别只能是一个级别之内。
                strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
                ret = RESPONSE_CODE::FORBIDDEN;
            } else {
                // 判断目标文件是否可以下载
                std::string final_path = pwd_ + "/" + copy_file_name_;
                struct stat file_stat;
                assert(stat(final_path.c_str(), &file_stat) != -1);  // 取出权限。
                if (!((file_stat.st_mode & (0777)) & S_IXOTH)) {  // 其他人不可执行，代表不是可分享的文件，无法下载。
                    strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
                    ret = RESPONSE_CODE::FORBIDDEN;
                } else {
                    strncpy(response_file_path_ + len, "/disk.html\0", strlen("/disk.html\0"));  // 响应的请求体对应的文件。
                    Utils::CopyFile(final_path, user_file_root_path_);
                    ret = RESPONSE_CODE::DISK_HTML;
                }
            }
        } else {  // 没有权限访问。
            strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
            ret = RESPONSE_CODE::FORBIDDEN;
        }
    }
    request = "/deleteDir";
    if (strncmp(p, request.c_str(), strlen(request.c_str())) == 0) {  // 如果是/download请求，判断是否已经登录了。
        if (login_state_ &&
            (Utils::InDir(user_file_root_path_, pwd_) || user_file_root_path_ == pwd_)) {  // 登录,并且当前的位置处于目标自己的根目录之下。
            strncpy(response_file_path_ + len, "/disk.html\0", strlen("/disk.html\0"));  // 响应的请求体对应的文件。
            const char *pos = strrchr(url_, '=');  // 找到等号的位置，操作的文件就在等号的右边。
            delete_file_name_ = pos + 1;
            if (delete_file_name_.find_first_of('/') != std::string::npos ||
                delete_file_name_.find_first_of('\\') != std::string::npos) {  // 说明存在恶意调用,限制操作级别只能是一个级别之内。
                strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
                ret = RESPONSE_CODE::FORBIDDEN;
            } else {
                Utils::DeleteDir(pwd_ + "/" + delete_file_name_);  // 删除对应的文件以后，再次走Disk.html的流程。
                ret = RESPONSE_CODE::DISK_HTML;
            }
        } else {  // 没有权限访问。
            strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
            ret = RESPONSE_CODE::FORBIDDEN;
        }
    } else {  // 这一段需要特殊处理一下，因为/delete和/deleteDir是同样前缀，可能会误解。
        request = "/delete";
        if (strncmp(p, request.c_str(), strlen(request.c_str())) == 0) {  // 如果是/download请求，判断是否已经登录了。
            if (login_state_ &&
                (Utils::InDir(user_file_root_path_, pwd_) ||
                 user_file_root_path_ == pwd_)) {  // 登录,并且当前的位置处于目标自己的根目录之下。
                strncpy(response_file_path_ + len, "/disk.html\0", strlen("/disk.html\0"));  // 响应的请求体对应的文件。
                const char *pos = strrchr(url_, '=');  // 找到等号的位置，操作的文件就在等号的右边。
                delete_file_name_ = pos + 1;
                if (delete_file_name_.find_first_of('/') != std::string::npos ||
                    delete_file_name_.find_first_of('\\') != std::string::npos) {  // 说明存在恶意调用,限制操作级别只能是一个级别之内。
                    strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
                    ret = RESPONSE_CODE::FORBIDDEN;
                } else {
                    remove((pwd_ + "/" + delete_file_name_).c_str());  // 删除对应的文件以后，再次走Disk.html的流程。
                    ret = RESPONSE_CODE::DISK_HTML;
                }
            } else {  // 没有权限访问。
                strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
                ret = RESPONSE_CODE::FORBIDDEN;
            }
        }
    }
    request = "/enter";
    if (strncmp(p, request.c_str(), strlen(request.c_str())) == 0) {  // 如果是/download请求，判断是否已经登录了。
        if (login_state_) {  // 登录了就可以。
            strncpy(response_file_path_ + len, "/disk.html\0", strlen("/disk.html\0"));  // 响应的请求体对应的文件。
            const char *pos = strrchr(url_, '=');  // 找到等号的位置，操作的文件就在等号的右边。
            entry_dir_name_ = pos + 1;
            if (entry_dir_name_.find_first_of('/') != std::string::npos ||
                entry_dir_name_.find_first_of('\\') != std::string::npos) {  // 说明存在恶意调用,限制操作级别只能是一个级别之内。
                strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
                ret = RESPONSE_CODE::FORBIDDEN;
            } else {
                pwd_ = pwd_ + "/" + entry_dir_name_;
                ret = RESPONSE_CODE::DISK_HTML;
            }
        } else {  // 没有权限访问。
            strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
            ret = RESPONSE_CODE::FORBIDDEN;
        }
    }
    request = "/share";
    if (strncmp(p, request.c_str(), strlen(request.c_str())) == 0) {  // 如果是/share请求，判断是否已经登录了。
        if (login_state_ &&
            (Utils::InDir(user_file_root_path_, pwd_) || user_file_root_path_ == pwd_)) {  // 登录,并且当前的位置处于目标自己的根目录之下。
            strncpy(response_file_path_ + len, "/disk.html\0", strlen("/disk.html\0"));  // 响应的请求体对应的文件。
            const char *pos = strrchr(url_, '=');  // 找到等号的位置，操作的文件就在等号的右边。
            share_file_name_ = pos + 1;
            if (share_file_name_.find_first_of('/') != std::string::npos ||
                share_file_name_.find_first_of('\\') != std::string::npos) {  // 说明存在恶意调用,限制操作级别只能是一个级别之内。
                strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
                ret = RESPONSE_CODE::FORBIDDEN;
            } else {
                chmod((pwd_ + "/" + share_file_name_).c_str(),
                      S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH |
                      S_IWOTH | S_IXOTH);  // 将分享文件的操作放在这里,也就是新增S_IXOTH。
                ret = RESPONSE_CODE::DISK_HTML;
            }
        } else {  // 没有权限访问。
            strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
            ret = RESPONSE_CODE::FORBIDDEN;
        }
    }
    request = "/unshare";
    if (strncmp(p, request.c_str(), strlen(request.c_str())) == 0) {  // 如果是/unshare请求，判断是否已经登录了。
        if (login_state_ &&
            (Utils::InDir(user_file_root_path_, pwd_) || user_file_root_path_ == pwd_)) {  // 登录,并且当前的位置处于目标自己的根目录之下。
            strncpy(response_file_path_ + len, "/disk.html\0", strlen("/disk.html\0"));  // 响应的请求体对应的文件。
            const char *pos = strrchr(url_, '=');  // 找到等号的位置，操作的文件就在等号的右边。
            share_file_name_ = pos + 1;
            if (share_file_name_.find_first_of('/') != std::string::npos ||
                share_file_name_.find_first_of('\\') != std::string::npos) {  // 说明存在恶意调用,限制操作级别只能是一个级别之内。
                strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
                ret = RESPONSE_CODE::FORBIDDEN;
            } else {
                chmod((pwd_ + "/" + share_file_name_).c_str(),
                      S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP |
                      S_IROTH | S_IWOTH);  // 将取消分享文件的操作放在这里,也就是删除S_IXOTH。
                ret = RESPONSE_CODE::DISK_HTML;
            }
        } else {  // 没有权限访问。
            strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
            ret = RESPONSE_CODE::FORBIDDEN;
        }
    }
    request = "/back";
    if (strncmp(p, request.c_str(), strlen(request.c_str())) == 0) {  // 如果是/back请求，判断是否已经登录了。
        if (login_state_ && Utils::InDir(Config::get_singleton_()->http_connection_file_dir_root_path_,
                                         pwd_)) {  // 登录,并且当前的位置处于文件系统的根目录之下。
            strncpy(response_file_path_ + len, "/disk.html\0", strlen("/disk.html\0"));  // 响应的请求体对应的文件。
            pwd_ = pwd_.substr(0, pwd_.find_last_of("/"));  // 修改当前位置，删除最后一个/之后的所有内容
            ret = RESPONSE_CODE::DISK_HTML;
        } else {  // 没有权限访问。
            strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
            ret = RESPONSE_CODE::FORBIDDEN;
        }
    }
    request = "/mkdir";
    if (strncmp(p, request.c_str(), strlen(request.c_str())) == 0) {  // 如果是/mkdir请求，判断是否已经登录了。
        if (login_state_ && Utils::InDir(Config::get_singleton_()->http_connection_file_dir_root_path_,
                                         pwd_)) {  // 登录,并且当前的位置处于文件系统的根目录之下。
            strncpy(response_file_path_ + len, "/disk.html\0", strlen("/disk.html\0"));  // 响应的请求体对应的文件。
            const char *pos = strrchr(url_, '=');  // 找到等号的位置，操作的文件就在等号的右边。
            mkdir_dir_name_ = pos + 1;
            mkdir((pwd_ + "/" + mkdir_dir_name_).c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP |
                                                          S_IROTH | S_IWOTH);  // 在当前目录下创建对应的文件夹,并且设定好对应的权限。
            ret = RESPONSE_CODE::DISK_HTML;
        } else {  // 没有权限访问。
            strncpy(response_file_path_ + len, "/forbidden.html\0", strlen("/forbidden.html\0"));
            ret = RESPONSE_CODE::FORBIDDEN;
        }
    }
    request = "/resource_image";
    if (strncmp(p, request.c_str(), strlen(request.c_str())) == 0) {  // 如果是/resource_image，不需要登录，因为这是系统要用的资源文件。
        size_t equal_position = strrchr(url_, '=') - url_;  // 找到等号的位置，操作的文件就在等号的右边。
        size_t split_position = request.find_last_of('/');  // 找到分割符号的位置。
        if (split_position != 0) {  // 说明想要分级，这有可能是人为的，不正确。
            return RESPONSE_CODE::FORBIDDEN;
        }
        strncpy(response_file_path_, (Config::get_singleton_()->http_connection_resource_dir_root_path_ + "/").c_str(),
                Config::get_singleton_()->http_connection_resource_dir_root_path_.size() + 1);
        strncpy(response_file_path_ + strlen(response_file_path_), url_ + equal_position + 1,
                strlen(url_ + equal_position + 1));
        resource_file_name_ = url_ + equal_position + 1;
        ret = RESPONSE_CODE::RESOURCE_HTML;
    }
    if (ret != RESPONSE_CODE::NOT_FOUND) {  // 找到了目标文件，开始加载到内存中。
        stat(response_file_path_, &response_file_stat_);
        int fd = open(response_file_path_, O_RDONLY);
        map_response_file_address_ = (char *) mmap(0, response_file_stat_.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
    }
    return ret;
}

/**
 * 根据构造好的HTML，创建对应的数据包。请求头来源于response_code。
 * @param response_code
 * @return
 */
bool HTTPConnection::ConstructResponse(HTTPConnection::RESPONSE_CODE response_code) {
    switch (response_code) {
        case RESPONSE_CODE::DISK_HTML: {
            const char *status_phrase{"OK\0"};
            AddLine(200, status_phrase);
            std::string js_code = "<script>";
            std::string show_position = pwd_.substr(10);
            if (show_position.empty()) {  // 默认的根目录。
                show_position = "/";
            }
            js_code = js_code + "document.getElementById(\"pwd\").textContent = \"当前位置:" + show_position + "\";";
            if (Utils::InDir(Config::get_singleton_()->http_connection_file_dir_root_path_,
                             pwd_)) {  // pwd一定在文件系统的根目录之下，不能突破出去。
                js_code = js_code + "show_back_option('/back');";  // 只发送back，这样跳转到哪里，直接在构造请求体的逻辑之中修改pwd就行。
            }
            std::vector<std::string> names;
            std::vector<bool> shares;
            std::vector<bool> is_dirs;
            Utils::IteratorDir(pwd_, names, is_dirs, shares, user_file_root_path_, pwd_);
            for (int i = 0; i < names.size(); ++i) {
                if (pwd_ == user_file_root_path_ || Utils::InDir(user_file_root_path_, pwd_)) {  // 如果当前在用户的根目录或者就是根目录
                    if (is_dirs[i]) {
                        std::string del_href = "'deleteDir?file_name=" + names[i] + "',",
                                enter_href = "'enter?file_name=" + names[i] + "',",
                                share_option = shares[i] ? "'取消分享':'unshare?file_name=" + names[i] + "'" :
                                               "'分享':'share?file_name=" + names[i] + "'";
                        js_code = js_code + "append('" + names[i] + "', '" +
                                  (shares[i] ? "所有人可见" : "仅自己可见") + "', {" +
                                  "'删除':" + del_href + "'进入':" + enter_href + share_option + "},'','');";
                    } else {
                        std::string del_href = "'delete?file_name=" + names[i] + "',",
                                download_href = "'download?file_name=" + names[i] + "',",
                                share_option = shares[i] ? "'取消分享':'unshare?file_name=" + names[i] + "'" :
                                               "'分享':'share?file_name=" + names[i] + "'";
                        std::string img_name = "", video_name = "";
                        if (names[i].find(".mp4") != std::string::npos ||
                            names[i].find(".flv") != std::string::npos) {
                            video_name = names[i];
                        } else if (names[i].find(".jpg") != std::string::npos ||
                                   names[i].find(".png") != std::string::npos) {
                            img_name = names[i];
                        }
//                        img_name = "";  // 因为会异步加载，所以这种记录用户登录的方式不合时宜了，需要更换为使用session。每一个资源都会建立一个新的连接来请求。
                        video_name = "";  // 为了不加载video,不然要好久。
                        js_code = js_code + "append('" + names[i] + "', '" +
                                  (shares[i] ? "所有人可见" : "仅自己可见") + "', {" +
                                  "'删除':" + del_href + "'下载':" + download_href + share_option + "},'" + img_name +
                                  "','" + video_name + "');";
                    }
                } else {
                    if (is_dirs[i]) {  // 仅显示文件夹名、状态、进入
                        std::string enter_href = "'enter?file_name=" + names[i] + "'";
                        js_code = js_code + "append('" + names[i] + "', '" +
                                  (shares[i] ? "所有人可见" : "仅自己可见") + "', {" +
                                  +"'进入':" + enter_href + "},'','');";
                    } else {  // 显示文件名、状态、下载、拷贝
                        std::string download_href = "'download2copy?file_name=" + names[i] + "',",
                                copy_href = "'copy?file_name=" + names[i] + "'";
                        std::string img_name = "", video_name = "";
                        if (names[i].find(".mp4") != std::string::npos ||
                            names[i].find(".flv") != std::string::npos) {
                            video_name = names[i];
                        } else if (names[i].find(".jpg") != std::string::npos ||
                                   names[i].find(".png") != std::string::npos) {
                            img_name = names[i];
                        }
//                        img_name = "";  // 因为会异步加载，所以这种记录用户登录的方式不合时宜了，需要更换为使用session。
                        video_name = "";  // 为了不加载video,不然要好久。
                        js_code = js_code + "append('" + names[i] + "', '" +
                                  (shares[i] ? "所有人可见" : "仅自己可见") + "', {" +
                                  "'下载':" + download_href + "'拷贝':" + copy_href + "},'" + img_name + "', '" +
                                  video_name + "');";
                    }
                }
            }
            js_code = js_code + "</script>";
            AddHeader(response_file_stat_.st_size + js_code.size(), "text/html;utf-8");
            iov_[0].iov_base = write_buffer_;
            iov_[0].iov_len = write_buffer_next_idx_;
            iov_[1].iov_base = map_response_file_address_;
            iov_[1].iov_len = response_file_stat_.st_size;
            js_ = new char[js_code.size() + 1]{'\0'};
            new_js_ = true;
            strcpy(js_, js_code.c_str());
            iov_[2].iov_base = js_;
            iov_[2].iov_len = js_code.size();
            iov_count_ = 3;
            byte_to_send = write_buffer_next_idx_ + response_file_stat_.st_size + strlen(js_);
            return true;
        }
        case RESPONSE_CODE::DOWNLOAD: {
            const char *status_phrase{"OK\0"};
            AddLine(200, status_phrase);
            AddResponse("Content-Disposition: attachment; filename=\"%s\"\r\n",
                        Utils::UrlDecode(download_file_name_).c_str());
            stat((pwd_ + "/" + download_file_name_).c_str(), &response_file_stat_);  // 将需要下载的文件加载到内存中。
            int fd = open((pwd_ + "/" + download_file_name_).c_str(), O_RDONLY);
            map_response_file_address_ = (char *) mmap(0, response_file_stat_.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
            close(fd);
            if (download_file_name_.find(".jpg") != std::string::npos ||
                download_file_name_.find(".png") != std::string::npos) {
                AddHeader(response_file_stat_.st_size, "image/jpeg");
            } else if (download_file_name_.find(".mp4") != std::string::npos) {
                AddHeader(response_file_stat_.st_size, "video/mp4");
            } else if (download_file_name_.find(".flv") != std::string::npos) {
                AddHeader(response_file_stat_.st_size, "video/x-flv");
            } else {
                AddHeader(response_file_stat_.st_size, "text/plain");
            }
            iov_[0].iov_base = write_buffer_;
            iov_[0].iov_len = write_buffer_next_idx_;
            iov_[1].iov_base = map_response_file_address_;
            iov_[1].iov_len = response_file_stat_.st_size;
            iov_count_ = 2;
            byte_to_send = write_buffer_next_idx_ + response_file_stat_.st_size;
            return true;
        }
        case RESPONSE_CODE::OK: {
            const char *status_phrase{"OK\0"};
            AddLine(response_code, status_phrase);
            AddHeader(response_file_stat_.st_size, "text/html;utf-8");
            iov_[0].iov_base = write_buffer_;
            iov_[0].iov_len = write_buffer_next_idx_;
            iov_[1].iov_base = map_response_file_address_;
            iov_[1].iov_len = response_file_stat_.st_size;
            iov_count_ = 2;
            byte_to_send = write_buffer_next_idx_ + response_file_stat_.st_size;
            return true;
        }
        case RESPONSE_CODE::RESOURCE_HTML: {
            const char *status_phrase{"OK\0"};
            AddLine(200, status_phrase);
            AddResponse("Content-Disposition: inline; filename=\"%s\"\r\n",
                        Utils::UrlDecode(resource_file_name_).c_str());
            if (resource_file_name_.find(".jpg") != std::string::npos ||
                resource_file_name_.find(".png") != std::string::npos) {
                AddHeader(response_file_stat_.st_size, "image/jpeg");
            } else if (resource_file_name_.find(".mp4") != std::string::npos) {
                AddHeader(response_file_stat_.st_size, "video/mp4");
            } else if (resource_file_name_.find(".flv") != std::string::npos) {
                AddHeader(response_file_stat_.st_size, "video/x-flv");
            } else {
                AddHeader(response_file_stat_.st_size, "text/plain");
            }
            iov_[0].iov_base = write_buffer_;
            iov_[0].iov_len = write_buffer_next_idx_;
            iov_[1].iov_base = map_response_file_address_;
            iov_[1].iov_len = response_file_stat_.st_size;
            iov_count_ = 2;
            byte_to_send = write_buffer_next_idx_ + response_file_stat_.st_size;
            return true;
        }
        case RESPONSE_CODE::BAD_RESPONSE: {
            const char *status_phrase{"Bad Request\0"};
            AddLine(response_code, status_phrase);
            AddHeader(strlen("invalid"), "text/html;charset=8");
            if (!AddBody("invalid")) {
                return false;
            }
            break;
        }
        case RESPONSE_CODE::FORBIDDEN: {
            const char *status_phrase{"Forbidden\0"};
            AddLine(response_code, status_phrase);
            AddHeader(strlen("forbidden"), "text/html;charset=utf-8");
            if (!AddBody("forbidden")) {
                return false;
            }
            break;
        }
        case RESPONSE_CODE::NOT_FOUND: {
            const char *status_phrase{"Not Found\0"};
            AddLine(response_code, status_phrase);
            AddHeader(strlen("not found"), "text/html;charset=utf-8");
            if (!AddBody("not found")) {
                return false;
            }
            break;
        }
    }
    iov_[0].iov_base = write_buffer_;
    iov_[0].iov_len = write_buffer_next_idx_;
    iov_count_ = 1;
    byte_to_send = write_buffer_next_idx_;
    return true;
}

/**
 * 往buffer缓冲中写内容。
 * @param format
 * @param ...
 * @return
 */
bool HTTPConnection::AddResponse(const char *format, ...) {
    va_list va_list;
    va_start(va_list, format);
    int len = vsnprintf(write_buffer_ + write_buffer_next_idx_, write_buffer_max_len_ - write_buffer_next_idx_ - 1,
                        format, va_list);
    va_end(va_list);
    write_buffer_next_idx_ = write_buffer_next_idx_ + len;
    if (write_buffer_next_idx_ + len + 1 >= write_buffer_max_len_) {
        return false;
    }
    return true;
}

/**
 * 写入line。
 * @param response_code
 * @param response_phrase
 * @return
 */
bool HTTPConnection::AddLine(int response_code, const char *response_phrase) {
    Log::get_log_singleton_instance_()->write_log(Log::LOG_LEVEL::INFO, "服务器向 %s 返回请求 %s %d %s\r\n", inet_ntoa(client_address_.sin_addr), "HTTP/1.1", response_code, response_phrase);
    return AddResponse("%s %d %s\r\n", "HTTP/1.1", response_code, response_phrase);
}

/**
 * 新增响应头
 * @param content_length
 * @return
 */
bool HTTPConnection::AddHeader(int content_length, const std::string &content_type) {
    bool flag = true;
    flag &= AddResponse("Content-Length: %d\r\n", content_length);
    flag &= AddResponse("Content-Type: %s\r\n", content_type.c_str());
    flag &= AddResponse("Connection: %s\r\n", linger_ ? "keep-alive" : "close");
    flag &= AddResponse("\r\n");
    return flag;
}

/**
 * 新增响应体。
 * @param body
 * @return
 */
bool HTTPConnection::AddBody(const char *body) {
    return AddResponse("%s", body);
}

/**
 * 根据发送的内容，修改iov，因为iov不仅仅只有一个或者两个了，现在可以多个。
 * @param tmp
 * @param 多个缓存的起始地址
 */
void HTTPConnection::UpdateIov(int tmp, char *starts[]) {
    for (int i = 0; i < iov_count_; ++i) {
        if (tmp > 0 && iov_[i].iov_len > 0) {
            unsigned used = (iov_[i].iov_len < tmp) ? iov_[i].iov_len : tmp;
            iov_[i].iov_len -= used;
            iov_[i].iov_base = (char *) iov_[i].iov_base + used;  // 这里不能直接使用starts[i]，starts[i]其实从头到尾都没有发生过改变。
            tmp -= used;
        }
    }
}


HTTPConnection::HTTPConnection() {

}

HTTPConnection::~HTTPConnection() {
    close(client_fd_);
    close(epoll_fd_);
    delete[] read_buffer_;
    delete[] write_buffer_;
    delete[] response_file_path_;
    if (new_url_) {
        delete[]url_;
        new_url_ = false;
    }
    if (new_version_) {
        delete[]version_;
        new_version_ = false;
    }
    if (new_host_) {
        delete[]host_;
        new_host_ = false;
    }
    if (new_body_) {
        delete[] body_;
        new_body_ = false;
    }
    if (new_js_) {
        delete[] js_;
        new_js_ = false;
    }
}

/**
 * 当建立了一个连接一个都需要使用这个函数进行初始化。Init是在完成了一个连接的所有操作之后，进行状态的回调。
 * @param client_fd
 * @param epoll_fd
 * @param client_address
 */
void HTTPConnection::Establish(int client_fd, int epoll_fd, const sockaddr_in &client_address, const std::string &session) {
    this->session_ = session;
    ++WebServer::connection_num_;
    client_fd_ = client_fd;
    epoll_fd_ = epoll_fd;
    client_address_ = client_address;
    read_buffer_max_len_ = Config::get_singleton_()->http_connection_read_buffer_max_len_;
    read_buffer_ = new char[read_buffer_max_len_];
    memset(read_buffer_, '\0', read_buffer_max_len_);
    read_next_idx_ = 0;
    main_next_idx_ = 0;
    sub_next_idx_ = 0;
    level_trigger_ = Config::get_singleton_()->web_server_connection_level_trigger_;
    one_shot_ = Config::get_singleton_()->web_server_connection_one_shot_;
    sub_machine_state_ = SUB_MACHINE_STATE::GOOD_LINE;
    main_machine_state_ = MAIN_MACHINE_STATE::LINE;
    new_url_ = false;
    new_version_ = false;
    linger_ = false;
    body_length_ = 0;
    new_host_ = false;
    response_file_path_max_len_ = Config::get_singleton_()->http_connection_response_file_path_max_len_;
    response_file_path_ = new char[response_file_path_max_len_]{'\0'};
    strcpy(response_file_path_, Config::get_singleton_()->http_connection_html_dir_path_.c_str());
    write_buffer_max_len_ = Config::get_singleton_()->http_connection_write_buffer_max_len_;
    write_buffer_ = new char[write_buffer_max_len_]{'\0'};
    write_buffer_next_idx_ = 0;
    login_state_ = false;
    new_js_ = false;
    user_id_ = -1;
    boundary_ = "";
    blank_count_ = 0;
    upload_file_name_ = "";
    download_file_name_ = "";
    delete_file_name_ = "";
    share_file_name_ = "";
    entry_dir_name_ = "";
    copy_file_name_.clear();
    start_boundary_ = false;
    mines_count = 0;
    mkdir_dir_name_ = "";
    resource_file_name_ = "";
}

/**
 * 进行状态回调，回到刚建立连接的时候。
 */
void HTTPConnection::Init() {
    memset(read_buffer_, '\0', read_buffer_max_len_);
    read_next_idx_ = 0;
    main_next_idx_ = 0;
    sub_next_idx_ = 0;
    sub_machine_state_ = SUB_MACHINE_STATE::GOOD_LINE;
    main_machine_state_ = MAIN_MACHINE_STATE::LINE;
    if (new_url_) {
        delete[]url_;
        new_url_ = false;
    }
    if (new_version_) {
        delete[]version_;
        new_version_ = false;
    }
    if (new_host_) {
        delete[]host_;
        new_host_ = false;
    }
    if (new_body_) {
        delete[] body_;
        new_body_ = false;
    }
    if (new_js_) {
        delete[] js_;
        new_js_ = false;
    }
    linger_ = false;
    body_length_ = 0;
    memset(response_file_path_, '\0', response_file_path_max_len_);
    strcpy(response_file_path_, Config::get_singleton_()->http_connection_html_dir_path_.c_str());
    write_buffer_next_idx_ = 0;
    memset(write_buffer_, '\0', write_buffer_max_len_);
    boundary_.clear();
    blank_count_ = 0;
    upload_file_name_.clear();
    download_file_name_.clear();
    delete_file_name_.clear();
    share_file_name_.clear();
    entry_dir_name_.clear();
    copy_file_name_.clear();
    mkdir_dir_name_.clear();
    resource_file_name_.clear();
    start_boundary_ = false;
    mines_count = 0;
}

/**
 * 读取信息。
 * @return
 */
bool HTTPConnection::ReceiveMessage() {
    if (read_next_idx_ >= read_buffer_max_len_) {
        return false;
    }
    if (level_trigger_) {  // 水平触发读取一次。
        int byte = recv(client_fd_, read_buffer_ + read_next_idx_, read_buffer_max_len_ - read_next_idx_, 0);
        if (byte <= 0) {
            return false;
        }
        read_next_idx_ = read_next_idx_ + byte;
    } else {
        while (true) {
            int byte = recv(client_fd_, read_buffer_ + read_next_idx_, read_buffer_max_len_ - read_next_idx_,
                            0);  // 这里接收数据很搞，需要反复的回来接收，可以想办法更改一下。
            if (byte == 0) {
                return false;
            }
            if (byte == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {  // 可能是消息读完了，不算错误。
                    break;
                }
                return false;
            }
            read_next_idx_ = read_next_idx_ + byte;
        }
        return true;
    }
    return true;
}


/**
 * 对读取的内容进行处理解析，并构造好需要返回的内容，如果中间出现错误，直接返回false，将会直接关闭连接。
 * @return
 */
bool HTTPConnection::Process() {
    REQUEST_CODE request_code = AnalysisRequestMessage();
    RESPONSE_CODE response_code;
    if (request_code == REQUEST_CODE::TEMP_GOOD_REQUEST) {  // 还需要接续接收数据包，重新激活读事件,连接不需要删除。
        Utils::ModEpoll(client_fd_, epoll_fd_, true, level_trigger_, one_shot_);
        return true;
    } else if (request_code ==
               REQUEST_CODE::BAD_REQUEST) {  // 解析失败，直接删除连接,但是也应该给出页面，提示对方,但是不需要构造页面，因为构造页面的函数是固定解析成功才调用的。
        response_code = RESPONSE_CODE::BAD_RESPONSE;
    } else {
        response_code = ConstructorHtml();  // 构造页面，并根据构造的结果返回状态码，用于填充响应头。
    }
    if (!ConstructResponse(response_code)) {  // 如果添加到缓存的过程中有错误直接关闭连接，如果是大文件，并不会添加到缓存中，而是映射到内存中。
        WebServer::sort_timer_list_.DelTimer(WebServer::timers_[client_fd_]);  // 删除目标的计时器。
        Utils::DelEpoll(client_fd_, epoll_fd_);
        --WebServer::connection_num_;
        WebServer::session_map_[WebServer::connections_[client_fd_].session_].erase(client_fd_);  // 断开连接以后需要解除绑定。
        return false;
    }
    Utils::ModEpoll(client_fd_, epoll_fd_, false, level_trigger_, one_shot_);  // 告知主线程，开始写事件。
    return true;
}


/**
 * 解析request报文，并返回对应的代码。
 * @return
 */
HTTPConnection::REQUEST_CODE HTTPConnection::AnalysisRequestMessage() {
    sub_machine_state_ = SUB_MACHINE_STATE::GOOD_LINE;
    REQUEST_CODE request_code = REQUEST_CODE::TEMP_GOOD_REQUEST;
    char *line;
    std::string buffer;  // 用于将终止符也复制进去，可以使用memory代替。
    while (main_machine_state_ != MAIN_MACHINE_STATE::OVER) {  // 当主状态机已经Over那就正常退出循环，否则代表解析过程中存在问题。
        sub_machine_state_ = CutLine();
        if (sub_machine_state_ == SUB_MACHINE_STATE::BAD_LINE) {  // 截断都是失败的，那解析更不用说。
            return REQUEST_CODE::BAD_REQUEST;
        } else if (sub_machine_state_ == SUB_MACHINE_STATE::CONTINUE_LINE) {  // 如果还需要继续切割，那就不用解析了。
            return REQUEST_CODE::TEMP_GOOD_REQUEST;
        }
        line = read_buffer_ + main_next_idx_;  // 因为从状态机的标头移动的更快，所以根据快慢指针，可以取出其中的部分作为一行。所以下面的情况都是切割好的。开始进行解析。
        switch (main_machine_state_) {  // 根据主状态机的状态进行不同情况的解析。
            case MAIN_MACHINE_STATE::LINE:
                request_code = AnalysisLine(line);
                Log::get_log_singleton_instance_()->write_log(Log::LOG_LEVEL::INFO, "%s 发起了请求 %s", inet_ntoa(client_address_.sin_addr), url_);
                if (request_code == REQUEST_CODE::BAD_REQUEST) {  // 解析出现问题了，直接返回。
                    return request_code;
                }
                break;
            case MAIN_MACHINE_STATE::HEAD:
                request_code = AnalysisHead(line);  // 如果是继续解析，是不需要返回的，并没有出错。
                if (request_code == REQUEST_CODE::GOOD_REQUEST) {  // 解析任务完成，直接返回。
                    return request_code;
                }
                break;
            case MAIN_MACHINE_STATE::BODY:
                buffer = std::string(line, sub_next_idx_ - main_next_idx_);
                request_code = AnalysisBody(buffer);
                return request_code;  // 不管是解析成功还是需要继续收包，都是直接返回。
        }
        main_next_idx_ = sub_next_idx_;
    }
    return request_code;
}


/**
 * 发送缓冲区和请求体中的内容。
 * @return
 */
bool HTTPConnection::WriteHTTPMessage() {
    int tmp = 0;
    int flag = 10;
    while (true) {
        tmp = writev(client_fd_, iov_, iov_count_);
        if (tmp == -1 && byte_to_send > 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                sleep(1);
                continue;
            } else {
                return false;
            }
        }
        if (tmp > 0) {
            byte_have_send = byte_have_send + tmp;
            byte_to_send = byte_to_send - tmp;
        }
        char *starts[]{write_buffer_, map_response_file_address_, js_};
        UpdateIov(tmp, starts);  // 更新缓冲区。
        if (byte_to_send <= 0) {  // 发完了。
            if (iov_count_ > 1) {  // 需要解除内存中的映射。
                munmap(map_response_file_address_, response_file_stat_.st_size);
                map_response_file_address_ = 0;
            }
            if (linger_) {
                Init();
                Utils::ModEpoll(client_fd_, epoll_fd_, true, level_trigger_, one_shot_); // 重新激活读事件
                return true;
            } else {
                WebServer::sort_timer_list_.DelTimer(WebServer::timers_[client_fd_]);  // 删除目标的计时器。
                Utils::DelEpoll(client_fd_, epoll_fd_);
                WebServer::connection_num_--;
                WebServer::session_map_[WebServer::connections_[client_fd_].session_].erase(client_fd_);  // 断开连接以后需要解除绑定。
                return false;
            }
        }
    }
}


HTTPConnection &HTTPConnection::operator=(const HTTPConnection &other) {
    this->read_buffer_max_len_ = other.read_buffer_max_len_;
    memset(this->read_buffer_, '\0', other.read_buffer_max_len_);
    this->read_next_idx_ = 0;
    this->main_next_idx_ = 0;
    this->sub_next_idx_ = 0;
    this->sub_machine_state_ = SUB_MACHINE_STATE::GOOD_LINE;
    this->main_machine_state_ = MAIN_MACHINE_STATE::LINE;
    this->method_ = METHOD::GET;
    this->response_file_path_max_len_ = other.response_file_path_max_len_;
    this->write_buffer_max_len_ = other.write_buffer_max_len_;
    memset(write_buffer_, '\0', other.write_buffer_max_len_);
    this->write_buffer_next_idx_ = 0;
    this->login_state_ = other.login_state_;
    this->user_id_ = other.user_id_;
    this->username_ = other.username_;
    this->user_file_root_path_ = other.user_file_root_path_;
    this->pwd_ = other.pwd_;
    this->epoll_fd_ = other.epoll_fd_;
    this->level_trigger_ = other.level_trigger_;
    this->one_shot_ = other.one_shot_;
    return *this;
}