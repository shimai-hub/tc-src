#ifndef _API_COMMON_H_
#define _API_COMMON_H_
#include "cache_pool.h"
#include "db_pool.h"
#include "redis_keys.h"
// #include "tc_common.h"
#include "muduo/base/Logging.h"
#include "json/json.h"
#include <string>

#define FILE_NAME_LEN (256)    //文件名字长度
#define TEMP_BUF_MAX_LEN (512) //临时缓冲区大小
#define FILE_URL_LEN (512)     //文件所存放storage的host_name长度
#define HOST_NAME_LEN (30)     //主机ip地址长度
#define USER_NAME_LEN (128)    //用户名字长度
#define TOKEN_LEN (128)        //登陆token长度
#define MD5_LEN (256)          //文件md5长度
#define PWD_LEN (256)          //密码长度
#define TIME_STRING_LEN (25)   //时间戳长度
#define SUFFIX_LEN (8)         //后缀名长度
#define PIC_NAME_LEN (10)      //图片资源名字长度
#define PIC_URL_LEN (256)      //图片资源url名字长度

#define HTTP_RESP_OK 0
#define HTTP_RESP_FAIL 1           //
#define HTTP_RESP_USER_EXIST 2     // 用户存在
#define HTTP_RESP_DEALFILE_EXIST 3 // 别人已经分享此文件
#define HTTP_RESP_TOKEN_ERR 4      //  token验证失败
#define HTTP_RESP_FILE_EXIST 5     //个人已经存储了该文件

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

// 开启多线程
#define API_MYFILES_MUTIL_THREAD 0
#define API_LOGIN_MUTIL_THREAD  0
#define UNUSED(expr)                                                           \
    do {                                                                       \
        (void)(expr);                                                          \
    } while (0)


int TrimSpace(char *inbuf);
int QueryParseKeyValue(const char *query, const char *key, char *value,
                       int *value_len_p);
int GetFileSuffix(const char *file_name, char *suffix);
//验证登陆token，成功返回0，失败-1
int VerifyToken(char *user, char *token);

template <typename... Args>
std::string FormatString(const std::string &format, Args... args) {
    auto size = std::snprintf(nullptr, 0, format.c_str(), args...) +
                1; // Extra space for '\0'
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(),
                       buf.get() + size - 1); // We don't want the '\0' inside
}

string RandomString(const int len);
int VerifyToken(string &user_name, string &token);

#define SQL_MAX_LEN (512) // sql语句长度

//处理数据库查询结果，结果集保存在buf，只处理一条记录，一个字段,
//如果buf为NULL，无需保存结果集，只做判断有没有此记录 返回值：
// 0成功并保存记录集，1没有记录集，2有记录集但是没有保存，-1失败
int GetResultOneCount(CDBConn *db_conn, char *sql_cmd, int &count);
int GetResultOneStatus(CDBConn *db_conn, char *sql_cmd, int &shared_status);

// 检测是否存在记录，-1 操作失败，0:没有记录， 1:有记录
int CheckwhetherHaveRecord(CDBConn *db_conn, char *sql_cmd);

int GetUserFilesCountByUsername(string user_name, int &count);
//获取共享文件的数量
int GetShareFilesCount(int &count);

extern string s_dfs_path_client;
extern string s_storage_web_server_ip;
extern string s_storage_web_server_port;
extern string s_shorturl_server_address;
extern string s_shorturl_server_access_token;
using std::string;
int ApiInit();
//获取用户文件个数
int CacheSetCount(CacheConn *cache_conn, string key, int64_t count);
int CacheGetCount(CacheConn *cache_conn, string key, int64_t &count);
int CacheIncrCount(CacheConn *cache_conn, string key);
int CacheDecrCount(CacheConn *cache_conn, string key);
int DBGetUserFilesCountByUsername(CDBConn *db_conn, string user_name,
                                  int &count);
int DBGetShareFilesCount(CDBConn *db_conn, int &count);
int DBGetSharePictureCountByUsername(CDBConn *db_conn, string user_name,
                                     int &count);
int RemoveFileFromFastDfs(const char *fileid);


#endif