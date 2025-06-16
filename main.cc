#include <iostream>
#include <string>
#include <signal.h>

#include "muduo/net/TcpConnection.h"
#include "muduo/net/TcpServer.h"
#include "muduo/net/EventLoop.h"
#include "muduo/base/Logging.h"

#include "http_parser_wrapper.h"
#include "http_conn.h"
#include "config_file_reader.h"
#include "db_pool.h"
#include "cache_pool.h"

#include "api/api_upload.h"

using namespace muduo;
using namespace muduo::net;
using namespace std;

//为了http_server找到对应的http_conn
std::map<uint32_t, CHttpConnPtr> s_http_map;

// TcpServer具备基本的连接建立、数据读入、数据发送功能
// HttpServer完善了事件驱动、连接管理、数据处理的功能
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
        LOG_INFO << "onMessage: " << conn.get();

        uint32_t uuid = std::any_cast<uint32_t>(conn->getContext());
        CHttpConnPtr& http_conn = s_http_map[uuid];

        http_conn->OnRead(buf);

        
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

int main(int argc, char* argv[])
{
    
    std::cout  << argv[0] << "[conf ] "<< std::endl;
     

     // 默认情况下，往一个读端关闭的管道或socket连接中写数据将引发SIGPIPE信号。我们需要在代码中捕获并处理该信号，
    // 或者至少忽略它，因为程序接收到SIGPIPE信号的默认行为是结束进程，而我们绝对不希望因为错误的写操作而导致程序退出。
    // SIG_IGN 忽略信号的处理程序
    signal(SIGPIPE, SIG_IGN); //忽略SIGPIPE信号
    int ret = 0;
    char *str_tc_http_server_conf = NULL;
    if(argc > 1) {
        str_tc_http_server_conf = argv[1];  // 指向配置文件路径
    } else {
        str_tc_http_server_conf = (char *)"tc_http_server.conf";
    }
     std::cout << "conf file path: " <<  str_tc_http_server_conf << std::endl;
     // 读取配置文件
    CConfigFileReader config_file(str_tc_http_server_conf);     //读取配置文件



    char *dfs_path_client = config_file.GetConfigName("dfs_path_client"); // /etc/fdfs/client.conf
    char *storage_web_server_ip = config_file.GetConfigName("storage_web_server_ip"); //后续可以配置域名
    char *storage_web_server_port = config_file.GetConfigName("storage_web_server_port");

    
     // 初始化mysql、redis连接池，内部也会读取读取配置文件tc_http_server.conf
    CacheManager::SetConfPath(str_tc_http_server_conf); //设置配置文件路径
    CacheManager *cache_manager = CacheManager::getInstance();
    if (!cache_manager) {
        LOG_ERROR <<"CacheManager init failed";
        return -1;
    }

    // 将配置文件的参数传递给对应模块
    ApiUploadInit(dfs_path_client, storage_web_server_ip, storage_web_server_port, "", "");

    CDBManager::SetConfPath(str_tc_http_server_conf);   //设置配置文件路径
    CDBManager *db_manager = CDBManager::getInstance();
    if (!db_manager) {
        LOG_ERROR <<"DBManager init failed";
        return -1;
    }


    
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