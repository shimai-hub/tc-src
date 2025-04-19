#include <iostream>
#include <string>

#include "muduo/net/TcpConnection.h"
#include "muduo/net/TcpServer.h"
#include "muduo/net/EventLoop.h"
#include "muduo/base/Logging.h"

#include "http_parser_wrapper.h"

#include "http_conn.h"

using namespace muduo;
using namespace muduo::net;
using namespace std;

//为了http_server找到对应的http_conn
std::map<uint32_t, CHttpConnPtr> s_http_map;

class HttpServer
{
public:
    HttpServer(EventLoop* loop, const InetAddress &addr, const std::string &name, int num_event_loops)
    :loop_(loop),
    server_(loop, addr, name)
    {
        server_.setConnectionCallback(std::bind(&HttpServer::onConnection, this, std::placeholders::_1)); 
        server_.setMessageCallback(std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        server_.setWriteCompleteCallback(std::bind(&HttpServer::onWriteComplete, this, std::placeholders::_1));    
        server_.setThreadNum(num_event_loops);
    }

    void start()
    {
        server_.start();
    }
private:
    void onConnection(const TcpConnectionPtr& conn){
        if(conn->connected()){

            LOG_INFO << "onConnection new conn" << conn.get();

            uint32_t uuid = conn_uuid_generator_++;
            conn->setContext(uuid);

            //触发callback，建立新连接时创建http_conn
            CHttpConnPtr http_conn = std::make_shared<CHttpconn>(conn);
            s_http_map.insert({uuid, http_conn});


        }
        else{
            LOG_INFO << "onConnection disable conn" << conn.get();
            conn_uuid_generator_--;
            uint32_t uuid = std::any_cast<uint32_t>(conn->getContext());
            s_http_map.erase(uuid);

        }
    }
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time){
        LOG_INFO << "onMessage" << conn.get();

        uint32_t uuid = std::any_cast<uint32_t>(conn->getContext());
        CHttpConnPtr& http_conn = s_http_map[uuid];

        http_conn->OnRead(buf);

        // LOG_INFO << "get msg:\n" << in_buf;
        conn->shutdown();
        
    }
    void onWriteComplete(const TcpConnectionPtr& conn){
        LOG_INFO << "onWriteComplete" << conn.get();
    }

    TcpServer server_; //回调，连接/断开，读入完成，写入完成
    EventLoop* loop_ = nullptr;
    //不同线程调用connection，用原子
    std::atomic<uint32_t> conn_uuid_generator_;
};

int main()
{
    std::cout << "hello tuchuang\n";

    uint16_t http_bind_port = 8081;
    const char* http_bind_ip = "0.0.0.0";
    uint32_t num_event_loops = 4;

    EventLoop loop;
    InetAddress addr(http_bind_ip, http_bind_port);
    HttpServer server(&loop, addr, "http_srv", num_event_loops);

    server.start();
    loop.loop();

    return 0;
}