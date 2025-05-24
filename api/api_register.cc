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
    user_name = root["userName"].asString();

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
int registerUser(string& user_name, string& nick_name, string& pwd, string& phone,string& email){
    //0，成功， 1，失败， 2，用户存在
    int ret = 0;
    uint32_t user_id = 0;

    //取出与数据库的连接
    CDBManager* db_manager = CDBManager::getInstance();
    CDBConn* db_conn = db_manager->GetDBConn("tuchuang_master");
    if(!db_conn){
        LOG_ERROR << "GetDBConn(tuchuang_master) failed";
        return -1;
    }
    
    //组织sql，查询数据库
    string str_sql = FormatString("select id from user_info where user_name='%s'", user_name.c_str());
    CResultSet* result_set = db_conn->ExecuteQuery(str_sql.c_str());
    //获取结果返回
    if(result_set && result_set->Next()){
        //存在，警告返回
        LOG_WARN << "id: " << result_set->GetInt("id") << ", user_name: " << user_name << " 已存在";
        ret = 2;
    }
    else{
        //不存在，写入返回
        time_t now;
        char create_time[TIME_STRING_LEN];
        now = time(NULL);
        strftime(create_time, TIME_STRING_LEN, "%Y-%m-%d %H-%M:%S",localtime(&now));
        str_sql = "insert into user_info "
                 "(`user_name`,`nick_name`,`password`,`phone`,`email`,`create_"
                 "time`) values(?,?,?,?,?,?)";
        LOG_INFO << "执行: " << str_sql;
        //预处理
        CPrepareStatement* stmt = new CPrepareStatement();
        if(stmt->Init(db_conn->GetMysql(), str_sql)){
            uint32_t index = 0;
            string c_time = create_time;
            stmt->SetParam(index++, user_name);
            stmt->SetParam(index++, nick_name);
            stmt->SetParam(index++, pwd);
            stmt->SetParam(index++, phone);
            stmt->SetParam(index++, email);
            stmt->SetParam(index++, c_time);
            bool bRet = stmt->ExecuteUpdate();
            if(bRet){
                ret = 0;
                user_id = stmt->GetInsertId();
                LOG_INFO << "user_id: " << user_id;
            }
            else{
                LOG_ERROR << "insert user_info failed." << str_sql;
                ret = 1;
            }
            delete stmt;
        }
    }

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
        LOG_ERROR << "post_data empty";
        ret = -1;
        goto END;
    }

    //反序列化
    ret = decodeRegisterJson(post_data, user_name, nick_name, pwd, phone, email);
    if(ret < 0){
        LOG_ERROR << "decodeRegisterJson failed";
        ret = -1;
        goto END;
    }

    //将注册信息写入数据库
    ret = registerUser(user_name, nick_name, pwd, phone, email);

END:
    if(ret == 0){
        encodeRegisterJson(HTTP_RESP_OK, resp_json);
    }else{
        encodeRegisterJson(HTTP_RESP_FAIL, resp_json);
    }
    return ret;
}