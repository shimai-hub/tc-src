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

int decodeBrowsePictJson(string& post_data, string& urlmd5){
    Json::Value root;
    Json::Reader reader;
    reader.parse(post_data, root);

    if(root["urlmd5"].isNull()){
        LOG_ERROR << "urlmd5 null";
        return -1;
    }
    urlmd5 = root["urlmd5"].asString();

    return 0;
}

int encodeBrowsePictJson(int code, string url, string& resp_json){
    Json::Value root;
    Json::FastWriter writer;
    root["code"] = code;
    if(code == 0){
        root["url"] = url;
    }

    resp_json = writer.write(root);

    return 0;
}

int decodePictListJson(string& post_data, string& user_name, string& token, int& start, int& count){
    Json::Value root;
    Json::Reader reader;
    reader.parse(post_data, root);

    if(root["user"].isNull()){
        LOG_ERROR << "user null";
        return -1;
    }
    user_name = root["user"].asString();

    if(root["token"].isNull()){
        LOG_ERROR << "token null";
        return -1;
    }
    token = root["token"].asString();

    if(root["start"].isNull()){
        LOG_ERROR << "start null";
        return -1;
    }
    start = root["start"].asInt();

    if(root["count"].isNull()){
        LOG_ERROR << "count null";
        return -1;
    }
    count = root["count"].asInt();

    return 0;
}


//这个文件是否存在不关注
int handleSharePicture(string& user_name, string& md5, string& file_name, string& resp_json){
    int ret = 0;
    CDBManager* db_manager = CDBManager::getInstance();
    CDBConn* db_conn = db_manager->GetDBConn("tuchuang_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);

    string urlmd5;
    urlmd5 = RandomString(32);

    char create_time[TIME_STRING_LEN];
    time_t now;
    now = time(NULL);
    strftime(create_time, TIME_STRING_LEN - 1, "%Y-%m-%d %H:%M:%S", localtime(&now));

    string key;
    string sql_cmd = FormatString("insert into share_picture_list (user, filemd5, file_name, urlmd5, `key`, pv, create_time) values('%s', '%s', '%s', '%s', '%s', '%d', '%s')" \
        , user_name.c_str(), md5.c_str(), file_name.c_str(), urlmd5.c_str(), key.c_str(), 0, create_time);
    LOG_INFO << "执行: " << sql_cmd;

    if(!db_conn->ExecuteCreate(sql_cmd.c_str())){
        LOG_ERROR << sql_cmd << "操作失败";
        ret = -1;
        goto END;
    }

END:
    if(ret == 0){
        encodeSharePictJson(HTTP_RESP_OK, urlmd5, resp_json);
    }
    else{
        encodeSharePictJson(HTTP_RESP_FAIL, urlmd5, resp_json);
    }
}

int handleBrowsePicture(string& urlmd5, string& resp_json){
    int ret = 0;
    string sql_cmd;
    string picture_url;
    string file_name;
    string user;
    string filemd5;
    string create_time;
    int pv = 0;

    //获取数据库连接
    CDBManager* db_manager = CDBManager::getInstance();
    CDBConn* db_conn = db_manager->GetDBConn("tuchuang_master");
    AUTO_REL_DBCONN(db_manager, db_conn);

    //查询urlMD5对应的md5
    sql_cmd = FormatString("select user, filemd5, file_name, pv, create_time from "
            "share_picture_list where urlmd5 = '%s'", urlmd5.c_str());
    CResultSet* result_set = db_conn->ExecuteQuery(sql_cmd.c_str());
    LOG_INFO << "执行: " << sql_cmd;
    if(result_set && result_set->Next()){
        user = result_set->GetString("user");
        file_name = result_set->GetString("file_name");
        filemd5 = result_set->GetString("filemd5");
        create_time = result_set->GetString("create_time");
        pv = result_set->GetInt("pv");
    }else{
        LOG_ERROR << sql_cmd << "操作失败";
        ret = -1;
        goto END;
    }

    //查询MD5对应的url
    sql_cmd = FormatString("select url from file_info where md5 = '%s'", filemd5.c_str());
    result_set = db_conn->ExecuteQuery(sql_cmd.c_str());
    LOG_INFO << "执行: " << sql_cmd;
    if(result_set && result_set->Next()){
        picture_url = result_set->GetString("url");
    }else{
        LOG_ERROR << sql_cmd << "操作失败";
        ret = -1;
        goto END;
    }

    //更新share_picture_list表的pv值,并返回用来访问的url
    pv++;
    sql_cmd = FormatString("update share_picture_list set pv = %d where urlmd5 = '%s'", pv, urlmd5.c_str());
    LOG_INFO << "执行: " << sql_cmd;
    if(!db_conn->ExecuteUpdate(sql_cmd.c_str())){
        LOG_ERROR << sql_cmd << "操作失败";
        ret = -1;
        goto END;
    }

END:
    if(ret == 0){
        encodeBrowsePictJson(HTTP_RESP_OK, picture_url, resp_json);
    } else{
        encodeBrowsePictJson(HTTP_RESP_FAIL, picture_url, resp_json);
    }

    if(result_set){
        delete result_set;
    }
    return ret;
}

int getSharePicturesCount(CDBConn *db_conn,  string &user_name, int &count) {
    int ret = 0;
 
    // 从mysql加载
    if (DBGetUserFilesCountByUsername(db_conn, user_name, count) < 0) {
        LOG_ERROR << "DBGetUserFilesCountByUsername failed";
        return -1;
    }

    return ret;
}

int handleGetSharePictList(const char* user, int start, int count, string& resp_json){
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    CResultSet *result_set = NULL;
    int total = 0;
    int file_count = 0;
    Json::Value root;
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("tuchuang_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);

    string temp_user = user;
    ret = getSharePicturesCount(db_conn, temp_user, total);
    if (ret < 0) {
        LOG_ERROR << "getSharePicturesCount failed";
        ret = -1;
        goto END;
    }
    if(total == 0){
        LOG_INFO << user << " share_file_list null";
        ret = 0;
        goto END;
    }
    // sql语句
    sprintf(
        sql_cmd,
        "select share_picture_list.user, share_picture_list.filemd5, share_picture_list.file_name,share_picture_list.urlmd5, share_picture_list.pv, \
        share_picture_list.create_time, file_info.size from file_info, share_picture_list where share_picture_list.user = '%s' and  \
        file_info.md5 = share_picture_list.filemd5 limit %d, %d", user, start, count);
    LOG_INFO << "执行: " <<  sql_cmd;
    result_set = db_conn->ExecuteQuery(sql_cmd);
    if (result_set) {
        // 遍历所有的内容
        // 获取大小
        Json::Value files;
        while (result_set->Next()) {
            Json::Value file;
            file["user"] = result_set->GetString("user");
            file["filemd5"] = result_set->GetString("filemd5");
            file["file_name"] = result_set->GetString("file_name");
            file["urlmd5"] = result_set->GetString("urlmd5");
            file["pv"] = result_set->GetInt("pv");
            file["create_time"] = result_set->GetString("create_time");
            file["size"] = result_set->GetInt("size");
            files[file_count] = file;
            file_count++;
        }
        if (file_count > 0)
            root["files"] = files;

        ret = 0;
        delete result_set;
    } else {
        ret = -1;
    }
END:
    if (ret != 0) {
        Json::Value root;
        root["code"] = 1;
    } else {
        root["code"] = 0;
        root["count"] = file_count;
        root["total"] = total;
    }
    resp_json = root.toStyledString();
    LOG_INFO << "resp_json: " << resp_json;

    return ret;   
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

        //反序列化
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

        //生成并返回urlmd5
        ret = handleSharePicture(user_name, md5, file_name, resp_json);

    }
    else if(strncmp(cmd, "browse", 6) == 0){
        //浏览请求，url改为完整下载路径url
        LOG_INFO << "post_data: " << post_data << ", urlmd5: " << urlmd5;

        //反序列化
        ret = decodeBrowsePictJson(post_data, urlmd5);
        if(ret == 0){
            //修改数据库，并返回浏览文件的url
            handleBrowsePicture(urlmd5, resp_json);
        }else{
            encodeBrowsePictJson(HTTP_RESP_FAIL, urlmd5, resp_json);
        }

    }
    else if(strncmp(cmd, "normal", 6) == 0){
        int start = 0;
        int count = 0;

        //请求该用户的分享列表
        LOG_INFO << "post_data: " << post_data;

        //反序列化
        ret = decodePictListJson(post_data, user_name, token, start, count);

        //获取该用户的share_picture_list文件信息，返回
        if(ret == 0){
            handleGetSharePictList(user_name.c_str(), start, count, resp_json);
        }

    }
    else{
        LOG_INFO << "no handle to " << cmd;
    }
    
    return ret;
}