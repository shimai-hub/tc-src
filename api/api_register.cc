#include "api_register.h"
#include "muduo/base/Logging.h"
#include <json/json.h>


using namespace std;

//json序列化
int encodeRegisterJson(int code, string& resp_json){
    Json::Value root;
    root["code"] = code;
    Json::FastWriter writer;
    resp_json = writer.write(root);
    return 0;
}

//json反序列化
int decodeRegisterJson(string& post_data, string& user_name, string& nick_name, string& pwd,
string& phone, string& email){
    bool res;
    Json::Value root;
    Json::Reader reader;
    res = reader.parse(post_data, root);
    if(!res){
        LOG_ERROR << "parse reg json failed";
        return -1;
    }

    //user_name
    if(root["userName"].isNull()){
        LOG_ERROR << "userName null";
        return -1;
    }
    user_name = root["user_Name"].asString();

    //nick_name
    if(root["nickName"].isNull()){
        LOG_ERROR << "nickName null";
        return -1;
    }
    nick_name = root["nickName"].asString();

    //pwd
    if(root["firstPwd"].isNull()){
        LOG_ERROR << "firstPwd null";
        return -1;
    }
    pwd = root["firstPwd"].asString();

    //phone
    if(root["phone"].isNull()){
        LOG_WARN << "phone null";
    } else {
        phone = root["phone"].asString();
    }

    //email
    if(root["email"].isNull()){
        LOG_WARN << "email null";
    } else {
        email = root["email"].asString();
    }

    return 0;
}

//json-mysql
int registerUser(string& user_name, string& nick_name, string& pwd, string& phone,
string& email){
    int ret = 0;
    return ret;
}

//http-json层，把用户信息注册，并填写返回结果
int ApiRegisterUser(string& post_data, string& resp_json){

    int ret = 0;
    string user_name;
    string nick_name;
    string pwd;
    string phone;
    string email;

    LOG_INFO << "post_data: " << post_data;

    if(post_data.empty()){
        LOG_ERROR << "decodeRegister failed";
        encodeRegisterJson(1, resp_json);
        return -1;
    }

    
    //注册
    //反序列化
    ret = decodeRegisterJson(post_data, user_name, nick_name, pwd, phone, email);
    if(ret < 0){
        encodeRegisterJson(1, resp_json);
        return -1;
    }

    //判断数据库中数据存在性及是否写入数据库
    ret = registerUser(user_name, nick_name, pwd, phone, email);
    encodeRegisterJson(ret, resp_json);

    return ret;
}