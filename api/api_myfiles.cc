#include "api_myfiles.h"
#include <string>

// /api/myfiles&cmd=count
// /api/myfiles&cmd=normal
// /api/myfiles&cmd=normal


//json, decode
int decodeCountJson(string& post_data, string& user_name, string& token){
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

    return 0;
}

int encodeCountJson(int code, int total, string& resp_json){
    Json::Value root;
    Json::FastWriter writer;
    root["code"] = code;
    if(code == 0){
        root["total"] = total;
    }

    resp_json = writer.write(root);
    return 0;
}

//json, decode
int decodeListJson(string& post_data, string& user_name, string& token, int& start, int& count){
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

    if(root["count"].isNull()){
        LOG_ERROR << "count null";
        return -1;
    }
    count = root["count"].asInt();

    if(root["start"].isNull()){
        LOG_ERROR << "start null";
        return -1;
    }
    start = root["start"].asInt();  

    return 0;
}

int encodeGetFileListFailedJson(string& resp_json){
    Json::Value root;
    Json::FastWriter writer;
    root["code"] = 1;

    resp_json = writer.write(root);
    return 0; 
}

int getUserFilesCount(CDBConn* db_conn, CacheConn* cache_conn, string& user_name, int& count){
    //封装sql
    int ret = 0;
    string sql_str = FormatString("select count(*) from user_file_list where user = '%s'", user_name.c_str());
    CResultSet* result_set = db_conn->ExecuteQuery(sql_str.c_str());

    if(result_set && result_set->Next()){
        count = result_set->GetInt("count(*)");
        LOG_INFO << "count: " << count;
        ret = 0;
        delete result_set;
    }
    else{
        ret = -1;
        LOG_ERROR << "操作" << sql_str << "失败";
    }

    return ret;
}

int handleUserFilesCount(string& user_name, int& count){
    CDBManager* db_manager = CDBManager::getInstance();
    CDBConn* db_conn = db_manager->GetDBConn("tuchuang_master");
    AUTO_REL_DBCONN(db_manager, db_conn);
    // CacheManager* cache_manager = CacheManager::getInstance();
    // CacheConn* cache_conn = cache_manager->GetCacheConn("token");
    // AUTO_REL_CACHECONN(cache_manager, cache_conn);

    return getUserFilesCount(db_conn, NULL, user_name, count);
}

int getUserFilesList(string cmd, string& user_name, int& start, int& count, string& resp_json){
    LOG_INFO << "getUserFilesList into";
    int ret = 0;
    int total_count = 0;
    string sql_url;

    CDBManager* db_manager = CDBManager::getInstance();
    CDBConn* db_conn = db_manager->GetDBConn("tuchuang_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);

    ret = getUserFilesCount(db_conn, NULL, user_name, total_count);
    if(ret < 0){
        LOG_ERROR << "getUserFilesCount failed";
        ret = -1;
        return ret;
    }

    //该用户无文件，不用查询文件信息
    if(total_count == 0){
        Json::Value root;
        Json::FastWriter writer;
        root["code"] = 0;
        root["count"] = 0;
        root["total"] = 0;
        resp_json = writer.write(root);
        return 0;
    }

    //该用户有文件
    sql_url = FormatString("select user_file_list.*, file_info.url, file_info.size, file_info.type \
        from user_file_list, file_info where user = '%s' and file_info.md5 = user_file_list.md5 limit %d, %d",
        user_name.c_str(), start, count);
    
    CResultSet* result_set = db_conn->ExecuteQuery(sql_url.c_str());
    if(result_set){
        int file_index = 0;
        Json::Value root;
        Json::Value files;
        Json::FastWriter writer;
        root["code"] = 0;
        root["total"] = total_count;
        
        while(result_set->Next()){
            Json::Value file;
            file["user"] = result_set->GetString("user");
            file["md5"] = result_set->GetString("md5");
            file["create_time"] = result_set->GetString("create_time");
            file["file_name"] = result_set->GetString("file_name");
            file["shared_status"] = result_set->GetInt("share_status");
            file["pv"] = result_set->GetInt("pv");
            
            file["url"] = result_set->GetString("url");
            file["size"] = result_set->GetInt("size");
            file["type"] = result_set->GetString("type");

            files[file_index++] = file;
        }
        
        root["files"] = files;
        root["count"] = file_index;
        delete result_set;

        resp_json = writer.write(root);

        LOG_INFO << "resp_json: " << resp_json;
    }

    return ret;
}

int ApiMyfiles(string &url, string &post_data, string &resp_json){
    int ret = 0;
    char cmd[20] = {0};
    string user_name;
    string token;
    int total_count = 0;
    int start = 0;
    int count = 0;

    //解析Url
    QueryParseKeyValue(url.c_str(), "cmd", cmd, NULL);

    //根据url，获取cmd对应的数据库结果
    if(strncmp(cmd, "count", 5) == 0){
        //反序列化，获取登录信息
        ret = decodeCountJson(post_data, user_name, token);
        if(ret < 0){
            encodeCountJson(1, 0, resp_json);
            LOG_ERROR << "decodeCountJson failed";
            return -1;
        }

        //校验token
        ret = VerifyToken(user_name, token);
        if(ret < 0){
            encodeCountJson(1, 0, resp_json);
            LOG_ERROR << "VerifyToken failed";
            return -1;  
        }
        //获取文件数量
        ret = handleUserFilesCount(user_name, total_count);
        if(ret < 0){
            encodeCountJson(1, 0, resp_json);
        }


        encodeCountJson(0, total_count, resp_json);

        return ret;
    }
    else if(strncmp(cmd, "normal", 6) == 0){
        //反序列化
        ret = decodeListJson(post_data, user_name, token, start, count);
        if(ret < 0){
            encodeGetFileListFailedJson(resp_json);
            LOG_ERROR << "encodeGetFileListFailedJson failed";
            return -1;
        }

        //校验token
        ret = VerifyToken(user_name, token);
        if(ret < 0){
            encodeGetFileListFailedJson(resp_json);
            LOG_ERROR << "VerifyToken failed";
            return -1;  
        }

        //获取用户文件总个数
        ret = getUserFilesList(cmd, user_name, start, count, resp_json);
        if(ret < 0){
            LOG_INFO << "getUserFilesList failed";
            return -1;
        }   

        return ret;
    }
    else if(strncmp(cmd, "pvasc", 5) == 0){
        //返回文件列表（按下载量升序）

    }
    else if(strncmp(cmd, "pvdesc", 5) == 0){
        //返回文件列表（按下载量降序）

    }
    return ret;
}