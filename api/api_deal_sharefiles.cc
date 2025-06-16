#include "api_deal_sharefiles.h"

int decodeDealsharefileJson(string& post_data, string& user_name, string& md5,
                            string& filename) {
    bool res;
    Json::Value root;
    Json::Reader jsonReader;
    res = jsonReader.parse(post_data, root);
    if (!res) {
        LOG_ERROR << "parse reg json failed";
        return -1;
    }

    if (root["user"].isNull()) {
        LOG_ERROR << "user null";
        return -1;
    }
    user_name = root["user"].asString();

    if (root["md5"].isNull()) {
        LOG_ERROR << "md5 null";
        return -1;
    }
    md5 = root["md5"].asString();

    if (root["filename"].isNull()) {
        LOG_ERROR << "filename null";
        return -1;
    }
    filename = root["filename"].asString();

    return 0;
}

int encodeDealsharefileJson(int ret, string& resp_json) {
    Json::Value root;
    root["code"] = ret;

    Json::FastWriter writer;
    resp_json = writer.write(root);
    return 0;
}


//取消分享文件
int handleCancelShareFile(string &user_name, string &md5, string &filename) {
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    char fileid[512] = {0}; //md5 + filename

    
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("tuchuang_master");
    AUTO_REL_DBCONN(db_manager, db_conn);
     CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("ranking_list");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    // 组装fileid
    sprintf(fileid, "%s%s", md5.c_str(), filename.c_str());

    // 设置分享状态 user_file_list
    // 共享标志设置为0
    sprintf(sql_cmd,
            "update user_file_list set shared_status = 0 where user = '%s' and md5 = '%s' and file_name = '%s'",
            user_name.c_str(), md5.c_str(), filename.c_str());
    LOG_INFO << "执行: " << sql_cmd;;
    if (!db_conn->ExecuteUpdate(sql_cmd, false)) {
        LOG_ERROR << sql_cmd << " 操作失败";
        ret = -1;
        goto END;
    }
    // 删除share_file_list记录 
    sprintf(sql_cmd,
            "delete from share_file_list where user = '%s' and md5 = '%s' and file_name = '%s'",
            user_name.c_str(), md5.c_str(), filename.c_str());
    LOG_INFO << "执行: " << sql_cmd ;
    
    if (!db_conn->ExecuteDrop(sql_cmd)) {
        LOG_ERROR << sql_cmd << " 操作失败";
        ret = -1;
        goto END;
    }


    // 从redis删除对应的key
    //===2、redis记录操作
    //有序集合删除指定成员
    ret = cache_conn->ZsetZrem(FILE_PUBLIC_ZSET, fileid);
    if (ret != 0) {
        LOG_INFO << "执行: ZsetZrem 操作失败";
        goto END;
    }

     //从hash移除相应记录
    LOG_INFO << "Hdel FILE_NAME_HASH  " << fileid;
    ret = cache_conn->Hdel(FILE_NAME_HASH, fileid);
    if (ret < 0) {
        LOG_INFO << "执行: hdel 操作失败: ret = " << ret;
        goto END;
    }

END:

    if (ret == 0) {
        return (0);
    } else {
        return (1);
    }
}

//转存文件，0成功，1失败，5文件已存在
int handleSaveFile(string &user_name, string &md5, string &filename)
{
    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    //当前时间戳
    struct timeval tv;
    struct tm *ptm;
    char creat_time[128];
    int count;

    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("tuchuang_master");
    AUTO_REL_DBCONN(db_manager, db_conn);

    // 是否已经存在这个文件 user_file_list  md5 filename username
    //查看此用户，文件名和md5是否存在，如果存在说明此文件存在
    sprintf(sql_cmd,
            "select * from user_file_list where user = '%s' and md5 = '%s' and  file_name = '%s'",
            user_name.c_str(), md5.c_str(), filename.c_str());
    // 有记录返回1，错误返回-1，无记录返回0
    ret = CheckwhetherHaveRecord(db_conn, sql_cmd);
    if (ret == 1) { //如果有结果，说明此用户已有此文件
        LOG_ERROR << "user_name: " << user_name << ", filename: " << filename << ", md5: " << md5 << " 已存在";
        ret = 5;   //已经存在的
        goto END;
    }

    //文件信息表，查找该文件的计数器
    sprintf(sql_cmd, "select count from file_info where md5 = '%s'", md5.c_str());
    count = 0;
    ret = GetResultOneCount(db_conn, sql_cmd, count); //执行sql语句
    if (ret != 0) {
        LOG_ERROR << sql_cmd << " 操作失败";
        ret = 1;
        goto END;
    }
    // 1、修改file_info中的count字段，+1 （count 文件引用计数）
    sprintf(sql_cmd, "update file_info set count = %d where md5 = '%s'",
            count + 1, md5.c_str());
    if (!db_conn->ExecuteUpdate(sql_cmd)) {
        LOG_ERROR << sql_cmd << " 操作失败";
        ret = 1;
        goto END;
    }

    // 2、user_file_list插入一条数据

    //使用函数gettimeofday()函数来得到时间。它的精度可以达到微妙
    gettimeofday(&tv, NULL);
    ptm = localtime( &tv.tv_sec); //把从1970-1-1零点零分到当前时间系统所偏移的秒数时间转换为本地时间
    // strftime()
    // 函数根据区域设置格式化本地时间/日期，函数的功能将时间格式化，或者说格式化一个时间字符串
    strftime(creat_time, sizeof(creat_time), "%Y-%m-%d %H:%M:%S", ptm);

    sprintf(sql_cmd,
            "insert into user_file_list(user, md5, create_time, file_name, shared_status, pv) values ('%s', '%s', '%s', '%s', %d, %d)",
            user_name.c_str(), md5.c_str(), creat_time, filename.c_str(), 0, 0);
    if (!db_conn->ExecuteCreate(sql_cmd)) {
        LOG_ERROR << sql_cmd << " 操作失败";
        ret = 1;
        goto END;
    }

    ret = 0;

 END:
    return ret;

 }

//更新共享文件下载量
int handlePvFile(string &md5, string &filename) {
    // share_file_list pv + 1;   redis zset score + 1

    int ret = 0;
    char sql_cmd[SQL_MAX_LEN] = {0};
    char fileid[1024] = {0};
    int pv = 0;

    
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("tuchuang_master");
    AUTO_REL_DBCONN(db_manager, db_conn);
    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("ranking_list");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);
    
    //文件标示，md5+文件名
    sprintf(fileid, "%s%s", md5.c_str(), filename.c_str());

    // share_file_list pv + 1;
    //===1、mysql的下载量+1(mysql操作)
    // sql语句
    //查看该共享文件的pv字段
    sprintf(
        sql_cmd,
        "select pv from share_file_list where md5 = '%s' and file_name = '%s'",
        md5.c_str(), filename.c_str());
    LOG_INFO << "执行: " << sql_cmd;;
    CResultSet *result_set = db_conn->ExecuteQuery(sql_cmd);
    if (result_set && result_set->Next()) {
        pv = result_set->GetInt("pv");
    } else {
        LOG_ERROR << sql_cmd << " 操作失败";
        ret = 1;
        goto END;
    }

    //更新该文件pv字段，+1
    sprintf(sql_cmd,
            "update share_file_list set pv = %d where md5 = '%s' and file_name "
            "= '%s'",
            pv + 1, md5.c_str(), filename.c_str());
    LOG_INFO << "执行: " << sql_cmd;;
    if (!db_conn->ExecuteUpdate(sql_cmd, false)) {
        LOG_ERROR << sql_cmd << " 操作失败";
        ret = 1;
        goto END;
    }

    // zset 的member
    //===2、判断元素是否在集合中(redis操作)
    ret = cache_conn->ZsetExit(FILE_PUBLIC_ZSET, fileid);
    if (ret == 0) //不存在
    {                     //===4、如果不存在，从mysql导入数据
        //===5、redis集合中增加一个元素(redis操作)
        cache_conn->ZsetAdd(FILE_PUBLIC_ZSET, pv + 1, fileid);

        //===6、redis对应的hash也需要变化 (redis操作)
        //     fileid ------>  filename
        cache_conn->Hset(FILE_NAME_HASH, fileid, filename);
        ret = 0;
    } else if(ret == 1){
        ret = cache_conn->ZsetIncr(FILE_PUBLIC_ZSET,  fileid); 
        if (ret != 0) {
            ret = 1;
            LOG_ERROR << "ZsetIncr 操作失败";
        }
    }
    else 
    {
        ret = 1;
        goto END;
    }

END:

    return ret;
}

int ApiDealsharefile(string& url, string& post_data, string& resp_json){
    int ret = 0;
    char cmd[20] = {0};
    string user_name;
    string filename;
    string md5;

    QueryParseKeyValue(url.c_str(), "cmd", cmd, NULL);

    ret = decodeDealsharefileJson(post_data, user_name, md5, filename);
    LOG_INFO << "cmd: " << cmd << ", user_name:" << user_name << ", md5: "<< md5 << ", filename: " << filename;
    if (ret != 0) {
        encodeDealsharefileJson(1, resp_json);
        return 0;
    }

    if (strcmp(cmd, "cancel") == 0) //取消分享文件
    {
        ret = handleCancelShareFile(user_name, md5, filename);
    } else if (strcmp(cmd, "save") == 0) //转存文件
    {
        ret = handleSaveFile(user_name, md5, filename);
    } else if (strcmp(cmd, "pv") == 0) //更新文件下载量
    {
        ret = handlePvFile(md5, filename);
    }

    if (ret < 0)
        encodeDealsharefileJson(1, resp_json);
    else
        encodeDealsharefileJson(0, resp_json);

    return 0;

}