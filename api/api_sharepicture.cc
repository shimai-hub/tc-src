#include "api_sharepicture.h"

int decodeSharePictJson(string& post_data, string& token, string& md5, string& user_name, string& file_name){
    Json::Value root;
    Json::Reader reader;
    reader.parse(post_data, root);

    if(root["token"].isNull()){
        LOG_ERROR << "token null";
        return -1;
    }
    token = root["token"].asString();

    if(root["md5"].isNull()){
        LOG_ERROR << "md5 null";
        return -1;
    }
    md5 = root["md5"].asString();

    if(root["user"].isNull()){
        LOG_ERROR << "user null";
        return -1;
    }
    user_name = root["user"].asString();

    if(root["filename"].isNull()){
        LOG_ERROR << "filename null";
        return -1;
    }
    file_name = root["filename"].asString();

    return 0;
} 

int encodeSharePictJson(int code, string urlmd5, string& resp_json){
    Json::Value root;
    Json::FastWriter writer;
    root["code"] = code;
    if(code == 0){
        root["urlmd5"] = urlmd5;
    }

    resp_json = writer.write(root);

    return 0;
}

int handleSharepicture(string& user_name, string& md5, string& file_name, string& resp_json){
    int ret = 0;
    CDBManager* db_manager = CDBManager::getInstance();
    CDBConn* db_conn = db_manager->GetDBConn("tuchuang_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);

    string urlmd5;
    urlmd5 = RandomString(32);

    char create_time[TIME_STRING_LEN];
    time_t now;
    now = time(NULL);
    strftime(create_time, TIME_STRING_LEN - 1, "%Y-%m-%d %H:%m:%s", localtime(&now));

    string key;
    string sql_cmd = FormatString("insert into share_picture_list (user, filemd5, file_name, urlmd5, 'key'\
        , pv, create_time) value('%s', '%s', '%s', '%s', '%s', '%d', '%s')" \
        , user_name.c_str(), md5.c_str(), file_name.c_str(), urlmd5.c_str(), key.c_str(), 0, create_time);
    LOG_INFO << "执行: " << sql_cmd;

    if(!db_conn->ExecuteCreate(sql_cmd.c_str())){
        LOG_ERROR << sql_cmd << "操作失败";
        ret = -1;
        goto END;
    }

END:
    if(ret == 0){
        encodeSharePictJson(HTTP_RESP_FAIL, urlmd5, resp_json);
    }
    else{
        encodeSharePictJson(HTTP_RESP_OK, urlmd5, resp_json);
    }
}

int ApiSharePicture(string& url, string& post_data, string& resp_json){
    int ret = 0;
    char cmd[20] = {0};
    string token;
    string md5;
    string user_name;
    string file_name;
    string urlmd5;

    //解析url
    QueryParseKeyValue(url.c_str(), "cmd", cmd, NULL);

    
    if(strncmp(cmd, "share", 5) == 0){
        //分享，获取文件url

        //序列化
        ret = decodeSharePictJson(post_data, token, md5, user_name, file_name);
        if(ret < 0){
            encodeSharePictJson(HTTP_RESP_FAIL, urlmd5, resp_json);
            LOG_ERROR << "decodeSharePictJson failed";
            return -1;
        }

        //校验token
        ret = VerifyToken(user_name, token);
        if(ret < 0){
            encodeSharePictJson(HTTP_RESP_FAIL, urlmd5, resp_json);
            LOG_ERROR << "VerifyToken failed";
            return -1;
        }

        //查询文件MD5
        ret = handleSharepicture(user_name, md5, file_name, resp_json);

    }
    else if(strncmp(cmd, "browse", 6) == 0){
        //下载请求，url改为完整下载路径url
    }
    else if(strncmp(cmd, "normal", 6) == 0){
        //该用户的分享列表
    }
    else{
        LOG_INFO << "no handle to " << cmd;
    }
    
    return ret;
}