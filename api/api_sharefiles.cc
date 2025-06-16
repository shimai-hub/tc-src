#include "api_sharefiles.h"

//url == /api/sharefiles&cmd=count(normal/pvdesc)

int handleGetSharefilesCount(int& total){
    int ret = 0;

    CDBManager* db_manager = CDBManager::getInstance();
    CDBConn* db_conn = db_manager->GetDBConn("tuchuang_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);

    string sql_cmd = "select count(*) from share_file_list";
    CResultSet *result_set = db_conn->ExecuteQuery(sql_cmd.c_str());
    if(result_set && result_set->Next()){
        total = result_set->GetInt("count(*)");
        LOG_INFO << "total: " << total;
        ret = 0;
        delete result_set;
    }else{
        LOG_ERROR << sql_cmd << " 操作失败";
        ret = 1;
    }

    return ret;
}

int encodeSharefilesJson(int code, int total, string& resp_json){
    Json::Value root;
    root["code"] = code;
    if (code == 0) {
        root["total"] = total; // 正常返回的时候才写入token
    }
    Json::FastWriter writer;
    resp_json = writer.write(root);
    return 0;
}

int decodeShareFileslistJson(string& post_data, int& start, int& count){
    int ret = 0;
    Json::Value root;
    Json::Reader reader;
    ret = reader.parse(post_data, root);
    if(!ret){
        LOG_ERROR << "parse reg json failed ";
        return -1;
    }

    if(root["start"].empty()){
        LOG_ERROR << "start null";
        return -1;
    }
    start = root["start"].asInt();

    if (root["count"].isNull()) {
        LOG_ERROR << "count null";
        return -1;
    }
    count = root["count"].asInt();

    return 0;
}

int handleGetShareFilelist(int start, int count, string& resp_json){
    int ret = 0;
    string sql_cmd;
    int total = 0;

    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("tuchuang_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);
    
    CResultSet *result_set = NULL;
    int file_index = 0;
    Json::Value root, files;

    ret = handleGetSharefilesCount(total);
    if(ret != 0) {
        ret = -1;
        goto END;
    }

    sql_cmd = FormatString(
            "select share_file_list.*, file_info.url, file_info.size, file_info.type from file_info, \
            share_file_list where file_info.md5 = share_file_list.md5 limit %d, %d",
            start, count);
    LOG_INFO << "执行: " <<  sql_cmd;

    result_set = db_conn->ExecuteQuery(sql_cmd.c_str());

    if(result_set){
        file_index = 0;
        root["total"] = total;

        while(result_set->Next()){
            Json::Value file;
            file["user"] = result_set->GetString("user");
            file["md5"] = result_set->GetString("md5");
            file["file_name"] = result_set->GetString("file_name");
            file["share_status"] = result_set->GetInt("share_status");
            file["pv"] = result_set->GetInt("pv");
            file["create_time"] = result_set->GetString("create_time");
            file["url"] = result_set->GetString("url");
            file["size"] = result_set->GetInt("size");
            file["type"] = result_set->GetString("type");
            files[file_index++] = file;  
        }

        root["count"] = file_index;
        if(file_index > 0) {
            root["files"] = files;    
        } else {
            LOG_WARN << "no files";
        }
        ret = 0;
        delete result_set;

    }else {
        ret = -1;
    }

END:
    if (ret == 0) {
        root["code"] = 0;
    } else {
        root["code"] = 1;
    }
    resp_json = root.toStyledString();

    return ret;
}

//获取redis的共享文件下载榜
int handleGetRankingFilelist(int start, int count, string& resp_json){
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    int total = 0;
    char filename[512] = {0};
    int sql_num = 0;
    int redis_num = 0;
    int score = 0;
    int end = 0;
    RVALUES value = NULL;
    Json::Value root;
    Json::Value files;

    int file_count = 0;
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("tuchuang_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);
    CResultSet *pCResultSet = NULL;

    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("ranking_list");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    ret = handleGetSharefilesCount(total);
    if (ret != 0) {
        LOG_ERROR << sql_cmd << " 操作失败";
        ret = -1;
        goto END;
    }

    //将redis共享文件数量与mysql共享文件数量校对
    sql_num = total;
    redis_num = cache_conn->ZsetZcard(FILE_PUBLIC_ZSET);
    if (redis_num == -1) {
        LOG_ERROR << "ZsetZcard  操作失败";
        ret = -1;
        goto END;
    }
    LOG_INFO << "sql_num: " << sql_num << ", redis_num: " <<  redis_num;

    //两者数据不同步，删除redis，同步mysql到redis
    if(sql_num != redis_num){

        //清除redis
        cache_conn->Del(FILE_PUBLIC_ZSET);
        cache_conn->Del(FILE_NAME_HASH);

        //从mysql中导入
        //获取数据
        strcpy(sql_cmd, "select md5, file_name, pv from share_file_list order by desc");
        LOG_INFO << "执行: " << sql_cmd;
        pCResultSet = db_conn->ExecuteQuery(sql_cmd);
        if (!pCResultSet) {
            LOG_ERROR << sql_cmd << " 操作失败";
            ret = -1;
            goto END;
        }

        //导入
        while(pCResultSet->Next()){

            char fileid[1024] = {0};
            string md5 = pCResultSet->GetString("md5");
            string file_name = pCResultSet->GetString("file_name");
            int pv = pCResultSet->GetInt("pv");
            sprintf(fileid, "%s%s",md5.c_str(), file_name.c_str());

            //写入
            cache_conn->ZsetAdd(FILE_PUBLIC_ZSET, pv, fileid);
            cache_conn->Hset(FILE_NAME_HASH, fileid, file_name);

        }
        delete pCResultSet;
    }

    value = (RVALUES)calloc(count, VALUES_ID_SIZE);
    if(value == NULL){
        ret = -1;
        goto END;
    }
    end = start + count - 1;

    // 从redis中查询pv倒序对应的fileid, 获取从start到end的所有element
        // score也就是pv，是用来排序的，真正用的是排序后的fileid
    ret = cache_conn->ZsetZrevrange(FILE_PUBLIC_ZSET, start, end, value, file_count);
    if (ret != 0) {
        LOG_ERROR << "ZsetZrevrange 操作失败";
        ret = -1;
        goto END;
    }
    
    for(int i = 0; i < file_count; ++i){
        ret = cache_conn->Hget(FILE_NAME_HASH, value[i], filename);
        if (ret != 0) {
            LOG_ERROR << "hget  操作失败";
            ret = -1;
            goto END;
        }
        Json::Value file;
        file["filename"] = filename;

        int score = cache_conn->ZsetGetScore(FILE_PUBLIC_ZSET, value[i]);
        if(score == -1){
            LOG_ERROR << "ZsetGetScore  操作失败";
            ret = -1;
            goto END;
        }
        file["pv"] = score;
        files[i] = file;
    }
    ret = 0;

END:

    if(ret == 0) {
        root["code"] = 0;
        root["total"] = sql_num;
        root["count"]  = file_count;
        root["files"] = files;
    } else {
         root["code"] = 1;
    }
    resp_json = root.toStyledString();

    return ret;
}

int ApiShareFiles(string& url, string& post_data, string& resp_json){
    int ret = 0;
    char cmd[20] = {0};
    string user_name;
    string token;
    int total = 0;
    int count = 0;
    int start = 0;

    LOG_INFO << "post_data: " <<  post_data;

    QueryParseKeyValue(url.c_str(), "cmd", cmd, NULL);
    LOG_INFO << "cmd: " <<  cmd;

    if(strcmp(cmd, "count") == 0){
        ret = handleGetSharefilesCount(total);
        encodeSharefilesJson(ret, total, resp_json);

    }else if(strcmp(cmd, "normal") == 0){
        if (decodeShareFileslistJson(post_data, start, count) < 0){ 
            encodeSharefilesJson(1, 0, resp_json);
            return 0;
        }
        // 获取共享文件
        handleGetShareFilelist(start, count, resp_json);

    }else if(strcmp(cmd, "pvdesc") == 0){
        if (decodeShareFileslistJson(post_data, start, count) < 0){ //通过json包获取信息
            encodeSharefilesJson(1, 0, resp_json);
            return 0;
        }
        // 获取排行榜
        handleGetRankingFilelist(start, count, resp_json);

    }else{
        LOG_ERROR << "no handle for " << cmd;
    }
    
}