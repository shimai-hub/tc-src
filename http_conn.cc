

#include "http_conn.h"
#include "api/api_register.h"
#include "api/api_login.h"
#include "api/api_md5.h"
#include "api/api_upload.h"
#include "api/api_myfiles.h"
#include "api/api_sharepicture.h"
#include "api/api_dealfile.h"
#include "api/api_sharefiles.h"
#include "api/api_deal_sharefiles.h"

#include <atomic>
#include <any>

#define HTTP_RESPONSE_JSON_MAX 4096
#define HTTP_RESPONSE_JSON                                                     \
    "HTTP/1.1 200 OK\r\n"                                                      \
    "Connection:close\r\n"                                                     \
    "Content-Length:%d\r\n"                                                    \
    "Content-Type:application/json;charset=utf-8\r\n\r\n%s"

#define HTTP_RESPONSE_HTML                                                    \
    "HTTP/1.1 200 OK\r\n"                                                      \
    "Connection:close\r\n"                                                     \
    "Content-Length:%d\r\n"                                                    \
    "Content-Type:text/html;charset=utf-8\r\n\r\n%s"


#define HTTP_RESPONSE_BAD_REQ                                                     \
    "HTTP/1.1 400 Bad\r\n"                                                      \
    "Connection:close\r\n"                                                     \
    "Content-Length:%d\r\n"                                                    \
    "Content-Type:application/json;charset=utf-8\r\n\r\n%s"


CHttpconn::CHttpconn(TcpConnectionPtr tcp_conn):   
        tcp_conn_(tcp_conn)
    {
        uuid_ = std::any_cast<uint32_t>(tcp_conn_->getContext());
        LOG_INFO << "构造CHttpconn uuid: " << uuid_;
    }

    CHttpconn::~CHttpconn()
    {
        LOG_INFO << "析构CHttpconn uuid: " << uuid_;
    }

    void CHttpconn::OnRead(Buffer* buf)
    {
        const char* in_buf = buf->peek();
        uint32_t len = buf->readableBytes();

        //LOG MESSAGE
        LOG_INFO << "in_buf:";
        LOG_INFO << in_buf;
        
        //http报文解析
        CHttpParserWrapper http_parser;
        http_parser.ParseHttpContent(in_buf, len);
        if(http_parser.IsReadAll()){
            //parser
            string url = http_parser.GetUrlString();
            string content = http_parser.GetBodyContentString();
            // LOG_INFO << "url:" << url << ", content:" << content;

            //distribute
            if(strncmp(url.c_str(), "/api/reg", 8) == 0){
                _HandleRegisterRequest(url, content);
            }
            else if(strncmp(url.c_str(), "/api/login", 10) == 0){
                _HandleLoginRequest(url, content);
            }
            else if(strncmp(url.c_str(), "/api/md5", 8) == 0){
                _HandleMd5Request(url, content);
            }
            else if(strncmp(url.c_str(), "/api/upload", 11) == 0){
                _HandleUploadRequest(url, content);
            }
            else if(strncmp(url.c_str(), "/api/myfiles", 12) == 0){
                _HandleMyfilesRequest(url, content);
            }
            else if(strncmp(url.c_str(), "/api/sharepic", 13) == 0){
                _HandleShareRequest(url, content);
            }
            else if(strncmp(url.c_str(), "/api/dealfile", 13) == 0){
                _HandleDealfileRequest(url, content);
            }
            else if(strncmp(url.c_str(), "/api/sharefiles", 15) == 0){
                _HandleSharefilesRequest(url, content);
            }
            else if(strncmp(url.c_str(), "/api/dealsharefile", 18) == 0){
                _HandleDealsharefileRequest(url, content);
            }
            else{
                //echo
                LOG_INFO << "no handle for " << url;
                char* resp_content = new char[256];
                string str_json = "{\"code\":0}";
                uint32_t len_json = str_json.size();
                //暂时
                #define HTTP_RESPONSE_REQ       \
                    "HTTP/1.1 200 OK\r\n"       \
                    "Connection:close\r\n"      \
                    "Content-Length:%d\r\n"     \
                    "Content-Type::application/json,charset=utf-8\r\n\r\n%s"
                snprintf(resp_content, 256, HTTP_RESPONSE_REQ, len_json, str_json.c_str());
                tcp_conn_->send(resp_content);
                delete[] resp_content;          
            }   
        }
    }

    //_Handle函数负责数据传给api处理，将结果发回对端
//1.把用户信息注册进数据库 2.组织json_content返回
int CHttpconn::_HandleRegisterRequest(string& url, string& post_data){
    string resp_json;
    int ret = ApiRegisterUser(post_data, resp_json);

    char* http_body = new char[HTTP_RESPONSE_JSON_MAX];
    uint32_t ulen = resp_json.length();
    snprintf(http_body, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_JSON, ulen, resp_json.c_str());
    tcp_conn_->send(http_body);
    delete[] http_body;
    
    LOG_INFO << "uuid: " << uuid_ << ", http_send";

    return 0;
}
    
//1.将用户信息与注册信息对比，组织http报文 2.发送给客户端
int CHttpconn::_HandleLoginRequest(string& url, string& post_data){
    string resp_json;
    int ret = ApiLoginUser(post_data, resp_json);
    
    char* http_body = new char[HTTP_RESPONSE_JSON_MAX];
    uint32_t ulen = resp_json.length();
    snprintf(http_body, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_JSON, ulen, resp_json.c_str());

    tcp_conn_->send(http_body);

    delete[] http_body;
    LOG_INFO << "uuid" << uuid_ << ", http_send";
    return 0;
}

//1.将上传文件的md5值与文件列表md5对比，存在修改数据库，不存在调用upload，返回http结果给客户端
int CHttpconn::_HandleMd5Request(string& url, string& post_data){
    string resp_json;
    int ret = ApiMd5(post_data, resp_json);
    
    char* http_body = new char[HTTP_RESPONSE_JSON_MAX];
    uint32_t ulen = resp_json.length();
    snprintf(http_body, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_JSON, ulen, resp_json.c_str());

    tcp_conn_->send(http_body);

    delete[] http_body;
    LOG_INFO << "uuid" << uuid_ << ", http_send";
    return 0;
}

//1.将文件上传到nginx临时目录，修改数据库，返回http结果给客户端
int CHttpconn::_HandleUploadRequest(string& url, string& post_data){
    string resp_json;
    int ret = ApiUpload(post_data, resp_json);
    
    char* http_body = new char[HTTP_RESPONSE_JSON_MAX];
    uint32_t ulen = resp_json.length();
    snprintf(http_body, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_JSON, ulen, resp_json.c_str());

    tcp_conn_->send(http_body);

    delete[] http_body;
    LOG_INFO << "uuid" << uuid_ << ", http_send";
    return 0;
}

int CHttpconn::_HandleMyfilesRequest(string& url, string& post_data){
    string resp_json;
    int ret = ApiMyfiles(url, post_data, resp_json);
    
    char* http_body = new char[HTTP_RESPONSE_JSON_MAX];
    uint32_t ulen = resp_json.length();
    snprintf(http_body, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_JSON, ulen, resp_json.c_str());

    tcp_conn_->send(http_body);

    delete[] http_body;
    LOG_INFO << "uuid" << uuid_ << ", http_send";
    return 0;
}

int CHttpconn::_HandleShareRequest(string& url, string& post_data){
    string resp_json;
    int ret = ApiSharePicture(url, post_data, resp_json);
    
    char* http_body = new char[HTTP_RESPONSE_JSON_MAX];
    uint32_t ulen = resp_json.length();
    snprintf(http_body, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_JSON, ulen, resp_json.c_str());

    tcp_conn_->send(http_body);

    delete[] http_body;
    LOG_INFO << "uuid" << uuid_ << ", http_send";
    return 0;
}

int CHttpconn::_HandleDealfileRequest(string& url, string& post_data){
    string resp_json;
    int ret = ApiDealfile(url, post_data, resp_json);
    
    char* http_body = new char[HTTP_RESPONSE_JSON_MAX];
    uint32_t ulen = resp_json.length();
    snprintf(http_body, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_JSON, ulen, resp_json.c_str());

    tcp_conn_->send(http_body);

    delete[] http_body;
    LOG_INFO << "uuid" << uuid_ << ", http_send";
    return 0;
}

int CHttpconn::_HandleSharefilesRequest(string& url, string& post_data){
    string resp_json;
    int ret = ApiShareFiles(url, post_data, resp_json);
    
    char* http_body = new char[HTTP_RESPONSE_JSON_MAX];
    uint32_t ulen = resp_json.length();
    snprintf(http_body, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_JSON, ulen, resp_json.c_str());

    tcp_conn_->send(http_body);

    delete[] http_body;
    LOG_INFO << "uuid" << uuid_ << ", http_send";
    return 0;
}

int CHttpconn::_HandleDealsharefileRequest(string& url, string& post_data){
    string resp_json;
    int ret = ApiDealsharefile(url, post_data, resp_json);
    
    char* http_body = new char[HTTP_RESPONSE_JSON_MAX];
    uint32_t ulen = resp_json.length();
    snprintf(http_body, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_JSON, ulen, resp_json.c_str());

    tcp_conn_->send(http_body);

    delete[] http_body;
    LOG_INFO << "uuid" << uuid_ << ", http_send";
    return 0;
}
