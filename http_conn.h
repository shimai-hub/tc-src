#ifndef __HTTP_CONN__H
#define __HTTP_CONN__H

#include "http_parser_wrapper.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/net/Buffer.h"
#include "muduo/base/Logging.h"

using namespace muduo;
using namespace muduo::net;
using namespace std;

class CHttpconn : public std::enable_shared_from_this<CHttpconn>
{
public:
    CHttpconn(TcpConnectionPtr tcp_conn);
    virtual ~CHttpconn();
    void OnRead(Buffer* buf);
private:
    int _HandleRegisterRequest(string& url, string& post_data);
    int _HandleLoginRequest(string& url, string& post_data);
    int _HandleMd5Request(string& url, string& post_data);
    int _HandleUploadRequest(string& url, string& post_data);
    int _HandleMyfilesRequest(string&url, string& post_data);
    int _HandleShareRequest(string&url, string& post_data);

    TcpConnectionPtr tcp_conn_;
    uint32_t uuid_ = 0;
    CHttpParserWrapper http_parser;
};

using CHttpConnPtr = std::shared_ptr<CHttpconn>;






#endif