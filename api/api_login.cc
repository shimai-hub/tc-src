#include "api_login.h"
#include "muduo/base/Logging.h"
#include <json/json.h>
#include <uuid/uuid.h>

using namespace std;

//将用户信息与注册信息对比，并填写json_content

//json, encode
int encodeLoginJson(int code, string& token, string& resp_json){
    Json::Value root;
    Json::FastWriter writer;
    root["code"] = code;
    if(code == 0){
        root["token"] = token;
    }

    resp_json = writer.write(root);
    return 0;
}

//json, decode
int decodeLoginJson(string& post_data, string& user_name, string& pwd){
    Json::Value root;
    Json::Reader reader;
    reader.parse(post_data, root);

    if(root["user"].isNull()){
        LOG_ERROR << "user null";
        return -1;
    }
    user_name = root["user"].asString();
    
    if(root["pwd"].isNull()){
        LOG_ERROR << "pwd null";
        return -1;
    }
    pwd = root["pwd"].asString();

    return 0;
}

std::string generateUUID(){
    uuid_t uuid;
    uuid_generate_time(uuid);
    char uuidStr[40] = {0};
    uuid_unparse(uuid, uuidStr);
    return std::string(uuidStr);
}

int verifyUserPassword(string& user_name, string& pwd){
    int ret = 0;
    //从mysql连接池中取连接
    CDBManager* db_manager = CDBManager::getInstance();
    CDBConn* db_conn = db_manager->GetDBConn("tuchuang_master");
    AUTO_REL_DBCONN(db_manager, db_conn);

    //组织sql
    string strSql = FormatString("select password from user_info where user_name='%s'", user_name.c_str());
    CResultSet* result_set = db_conn->ExecuteQuery(strSql.c_str());
    if(result_set && result_set->Next()){
        //验证密码
        string password = result_set->GetString("password");
        LOG_INFO << "mysql-pwd: " << password << ", user-pwd: " <<  pwd;
        if(password == pwd){
            ret = 0;
        }
        else{
            LOG_INFO << "password uncorrect";
            ret = -1;
        }
    }
    else{
        ret = -1;
    }

    delete result_set;
    
    return ret;
}

int setToken(string& user_name, string& token){
    int ret = 0;
    
    //从连接池取出连接
    CacheManager* cache_manager = CacheManager::getInstance();
    CacheConn* cache_conn = cache_manager->GetCacheConn("token");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);
    
    //生成token
    token = generateUUID();

    //把token写入redis
    if(cache_conn){
        cache_conn->SetEx(token, 86400, user_name);
        LOG_INFO << "setToken " << token << "-" << user_name;
    }
    else{
        ret = -1;
    }

    //返回
    return ret;
}

//1.检查数据 2.反序列化 3.查询数据库判断数据正确性 4.填写json_content
int ApiLoginUser(string& post_data, string& resp_json){
    
    int ret = 0;
    string user_name;
    string pwd;
    string token;

    // LOG_INFO << "post_data: " << post_data;

    //判断数据是否为空
    if(post_data.empty()){
        LOG_ERROR << "decodeLogin failed";
        encodeLoginJson(1, token, resp_json);
        return -1;
    }

    //解析请求json
    ret = decodeLoginJson(post_data, user_name, pwd);
    if(ret < 0){
        encodeLoginJson(1, token, resp_json);
        return -1;
    }

    //判断登录信息是否正确
    ret = verifyUserPassword(user_name, pwd);
    if(ret < 0){
        LOG_ERROR << "verifyUserPassword failed";
        encodeLoginJson(1, token, resp_json);
        return -1;
    }

    ret = setToken(user_name, token);
    if(ret < 0){
        LOG_ERROR << "setToken failed";
        encodeLoginJson(1, token, resp_json);
        return -1;
    }

    //一切无误
    encodeLoginJson(0, token, resp_json);
    return 0;
}