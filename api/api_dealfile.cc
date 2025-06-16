#include "api_dealfile.h"

int decodeDealfileJson(string &post_data, string &user_name, string &token,
                       string &md5, string &file_name) {
    bool res;
    Json::Value root;
    Json::Reader jsonReader;
    res = jsonReader.parse(post_data, root);
    if (!res) {
        LOG_ERROR << "parse reg json failed ";
        return -1;
    }

    if (root["user"].isNull()) {
        LOG_ERROR << "user null";
        return -1;
    }
    user_name = root["user"].asString();

    if (root["token"].isNull()) {
        LOG_ERROR << "token null";
        return -1;
    }
    token = root["token"].asString();

    if (root["md5"].isNull()) {
        LOG_ERROR << "md5 null";
        return -1;
    }
    md5 = root["md5"].asString();

    if (root["filename"].isNull()) {
        LOG_ERROR << "filename null";
        return -1;
    }
    file_name = root["filename"].asString();

    return 0;
}

int encodeDealfileJson(int ret, string &resp_json) {
    Json::Value root;
    root["code"] = ret;
    Json::FastWriter writer;
    resp_json = writer.write(root);

    // LOG_INFO << "resp_json: " <<  resp_json;
    return 0;
}

int handleShareFile(string& user, string& md5, string& file_name){
    int ret = 0;
    int share_state = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    char fileid[1024] = {0};
    //拿到连接
    CDBManager* db_manager = CDBManager::getInstance();
    CDBConn* db_conn = db_manager->GetDBConn("tuchuang_master");
    AUTO_REL_DBCONN(db_manager, db_conn);

    CacheManager* cache_manager = CacheManager::getInstance();
    CacheConn* cache_conn = cache_manager->GetCacheConn("ranking_list");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    //拿到redis文件唯一标识 MD5+filename
    sprintf(fileid, "%s%s", md5.c_str(), file_name.c_str());

    //向redis'zset查询是否存在分享
    if(cache_conn){
        //ret=1，存在
        ret = cache_conn->ZsetExit(FILE_PUBLIC_ZSET, fileid);
    }else{
        ret = 0;
    }

    LOG_INFO << "fileid: " << fileid << ", ZsetExit: " << ret;

    //ret=1
    if(ret == 1){
        LOG_WARN << "该文件已被分享";
        share_state = 3;
        goto END;
    }

    //不存在，有被清洗的可能，再去mysql查询
    sprintf(sql_cmd, "select * from share_file_list where md5 = '%s' and file_name = '%s'",
                md5.c_str(), file_name.c_str());
    //1,有分享记录
    ret = CheckwhetherHaveRecord(db_conn, sql_cmd);
    if(ret == 1){
        //有分享记录，存入redis
        cache_conn->ZsetAdd(FILE_PUBLIC_ZSET, 0, fileid);
        cache_conn->Hset(FILE_NAME_HASH, fileid, file_name);
        LOG_WARN << "该文件已被分享";
        share_state = 3;
        goto END;
    }

    //该文件无分享记录,进行分享，更新mysql和redis
    sprintf(sql_cmd, "update user_file_list set shared_status = 1 where user = '%s' and "
            "md5 = '%s' and file_name = '%s'", user.c_str(), md5.c_str(), file_name.c_str());
    LOG_INFO << "执行 " << sql_cmd;
    if (!db_conn->ExecuteUpdate(sql_cmd, false)) {
        LOG_ERROR << sql_cmd << " 操作失败";
        
        share_state = 1;
        goto END;
    }

    time_t now;
    char create_time[TIME_STRING_LEN];
    //获取当前时间
    now = time(NULL);
    strftime(create_time, TIME_STRING_LEN - 1, "%Y-%m-%d %H:%M:%S", localtime(&now));

    //插入一条share_file_list记录
    sprintf(sql_cmd, "insert into share_file_list (user, md5, create_time, file_name, pv) values ('%s', '%s', '%s', '%s', %d)",
            user.c_str(), md5.c_str(), create_time, file_name.c_str(), 0);
    if (!db_conn->ExecuteCreate(sql_cmd)) {
        LOG_ERROR << sql_cmd << " 操作失败";
        share_state = 1;
        goto END;
    }

    //设置redis 
    // redis保存此文件信息
    cache_conn->ZsetAdd(FILE_PUBLIC_ZSET, 0, fileid);
    if (cache_conn->Hset(FILE_NAME_HASH, fileid, file_name) < 0) {
        LOG_WARN << "Hset FILE_NAME_HASH failed";
    }
    share_state = 0;

END:
    return share_state;
}

int handleDeleteFile(string& user, string& md5, string& file_name){
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    char fileid[1024] = {0};
    int count = 0;
    int share_status = 0;
    int redis_has_record = 0;

    CDBManager* db_manager = CDBManager::getInstance();
    CDBConn* db_conn = db_manager->GetDBConn("tuchuang_master");
    AUTO_REL_DBCONN(db_manager, db_conn);

    CacheManager* cache_manager = CacheManager::getInstance();
    CacheConn* cache_conn = cache_manager->GetCacheConn("ranking_list");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    //查看是否分享过，分享过要从share_file_list和HASH、ZSET开始删除
    sprintf(sql_cmd, "select shared_status from user_file_list where user = '%s' and md5 = '%s' and file_name = '%s'",
            user.c_str(), md5.c_str(), file_name.c_str());
    LOG_INFO << "执行: " << sql_cmd;

    ret = GetResultOneStatus(db_conn, sql_cmd, share_status);
    if(ret == 0){
        LOG_INFO << "GetResultOneStatus share = " << share_status;
    }else{
         LOG_ERROR << "GetResultOneStatus" << " 操作失败";
        ret = -1;
        goto END;
    }
    
    if(share_status == 1){
        //分享过

        //redis- zset,hash
        sprintf(fileid, "%s%s", md5.c_str(), file_name.c_str());
        //有序集合删除记录
        cache_conn->ZsetZrem(FILE_PUBLIC_ZSET, fileid);
        //hash删除记录
        cache_conn->Hdel(FILE_NAME_HASH, fileid);

        //mysql, share_file_list
        sprintf(sql_cmd, "delete from share_file_list where user = '%s' and md5 = '%s' and file_name = '%s'",
                user.c_str(), md5.c_str(), file_name.c_str());
        LOG_INFO << "执行: " << sql_cmd;

        if(!db_conn->ExecuteDrop(sql_cmd)){
            LOG_ERROR << sql_cmd << " 操作失败";
            ret = -1;
            goto END;
        }
    }
    
    //mysql- user_file_list
    sprintf(sql_cmd, "delete from user_file_list where user = '%s' and md5 = '%s' and file_name = '%s'",
            user.c_str(), md5.c_str(), file_name.c_str());
    LOG_INFO << "执行: " << sql_cmd;

    if(!db_conn->ExecuteDrop(sql_cmd)){
        LOG_ERROR << sql_cmd << "操作失败";
        ret = -1;
        goto END;
    }

    //mysql- file_info
    //查询文件是否被引用
    sprintf(sql_cmd, "select count from file_info where md5 = '%s'",
            md5.c_str());
    LOG_INFO << "执行: " << sql_cmd;
    count = 0;
    ret = GetResultOneCount(db_conn, sql_cmd, count);
    LOG_INFO << "ret: {}, count: " <<  ret, count;
    if(ret != 0){
        LOG_ERROR << sql_cmd << " 操作失败";
        ret = -1;
        goto END;
    }

    //被引用
    if(count > 0){
        //file_info 文件引用次数-1
        count -= 1;
        sprintf(sql_cmd, "update file_info set count=%d where md5 = '%s'", count, md5.c_str());
        if(!db_conn->ExecuteUpdate(sql_cmd)){
            LOG_ERROR << sql_cmd << " 操作失败";
            ret = -1;
            goto END;
        }
    }

    //0人引用,删除file_info的文件信息，并在文件系统中删除存储文件
    if(count == 0){
        //拿到fileid
        sprintf(sql_cmd, "select file_id from file_info where md5 = '%s'", md5.c_str());
        string fileid;
        CResultSet* result_set = db_conn->ExecuteQuery(sql_cmd);
        if(result_set->Next()){
            fileid = result_set->GetString("file_id");
        }

        //delete from file_info
        sprintf(sql_cmd, "delete from file_info where md5 = '%s'",
                md5.c_str());

        if(!db_conn->ExecuteDrop(sql_cmd)){
                LOG_ERROR << sql_cmd << " 操作失败";
                ret = -1;
                goto END;
        }

        //delete from storage
        ret = RemoveFileFromFastDfs(fileid.c_str());
        if (ret != 0) {
            LOG_INFO << "RemoveFileFromFastDfs err: " <<  ret;
            ret = -1;
            goto END;
        }

    }
    
    ret = 0;

END:
    if(ret == 0){
        return 0;
    }
    else{
        return 1;
    }
}

static int handlePvFile(string &user, string &md5, string &filename) {
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    int pv = 0;
    
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("tuchuang_master");
    AUTO_REL_DBCONN(db_manager, db_conn);

    //查看该文件的pv字段
    sprintf(sql_cmd,
            "select pv from user_file_list where user = '%s' and md5 = '%s' and file_name = '%s'",
            user.c_str(), md5.c_str(), filename.c_str());
     LOG_INFO << "执行: " <<  sql_cmd;

    CResultSet *result_set = db_conn->ExecuteQuery(sql_cmd);
    if (result_set && result_set->Next()) {
        pv = result_set->GetInt("pv");
    } else {
        LOG_ERROR << sql_cmd << " 操作失败";
        ret = -1;
        goto END;
    }

    //更新该文件pv字段，+1
    sprintf(sql_cmd,  "update user_file_list set pv = %d where user = '%s' and md5 = "
            "'%s' and file_name = '%s'",
            pv + 1, user.c_str(), md5.c_str(), filename.c_str());
    LOG_INFO << "执行: " <<  sql_cmd;
    if (!db_conn->ExecuteUpdate(sql_cmd)) {
        LOG_ERROR << sql_cmd << " 操作失败";
        ret = -1;
        goto END;
    }    
    
END:

    if (ret == 0) {
        return (0);
    } else {
        return (1);
    }
}


int ApiDealfile(string& url, string& post_data, string& resp_json){
    char cmd[20] = {0};
    string user_name;
    string token;
    string md5;
    string file_name;
    int ret = 0;

    //解析cmd
    QueryParseKeyValue(url.c_str(), "cmd", cmd, NULL);
    LOG_INFO << "cmd: " << cmd;

    //分配任务给对应的cmd处理函数
    if(strncmp(cmd, "share", 5) == 0){      //将分享文件的信息写入数据库
        //反序列化
        if(decodeDealfileJson(post_data, user_name, token, md5, file_name) < 0){
            encodeDealfileJson(1, resp_json);
        }

        //token校验

        //检查是否分享过，执行对应分享逻辑（写入数据库）
        ret = handleShareFile(user_name, md5, file_name);
        encodeDealfileJson(ret, resp_json);

    }
    else if(strncmp(cmd, "del", 3) == 0){
        //反序列化
        if(decodeDealfileJson(post_data, user_name, token, md5, file_name) < 0){
            encodeDealfileJson(1, resp_json);
        }

        //token校验

        //检查是否存在分享文件，执行对应删除逻辑（从数据库删除）
        ret = handleDeleteFile(user_name, md5, file_name);
        encodeDealfileJson(ret, resp_json);

    }
    else if(strncmp(cmd, "pv", 2) == 0){
        //反序列化
        if(decodeDealfileJson(post_data, user_name, token, md5, file_name) < 0){
            encodeDealfileJson(1, resp_json);
        }

        //token校验

        //检查是否存在分享文件，执行对应删除逻辑（从数据库删除）
        ret = handlePvFile(user_name, md5, file_name);
        encodeDealfileJson(ret, resp_json);

    }
    else{
        LOG_ERROR << "no handle for cmd " << cmd;
    }

    return ret;
}