#include "api_login.h"
#include "muduo/base/Logging.h"
#include <json/json.h>

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

    if(root["userName"].isNull()){
        LOG_ERROR << "userName null";
        return -1;
    }
    user_name = root["userName"].asString();

    if(root["pwd"].isNull()){
        LOG_ERROR << "userName null";
        return -1;
    }
    pwd = root["pwd"].asString();

    return 0;
}

int verifyUserPassword(string& user_name, string& pwd){
    int ret = 0;
    return ret;
}

int setToken(string& user_name, string& token){
    int ret = 0;
    token = "1234";
    return ret;
}

//1.检查数据 2.反序列化 3.查询数据库判断数据正确性 4.填写json_content
int ApiLoginUser(string& post_data, string& resp_json){
    
    int ret = 0;
    string user_name;
    string pwd;
    string token;

    LOG_INFO << "post_data: " << post_data;

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