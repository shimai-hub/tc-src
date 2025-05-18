#include "api_upload.h"
#include <json/json.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

//获取fastDFS中的文件信息
int getFullUrlByFileid(char* fileid, char* fdfs_file_url){

    if(s_storage_web_server_ip.empty()){
        LOG_ERROR << "s_storage_web_server_ip is empty";
        return -1;
    }

    int ret = 0;

    char* p = NULL;
    char* q = NULL;
    char* k = NULL;

    char fdfs_file_stat_buf[TEMP_BUF_MAX_LEN] = {0};
    char fdfs_file_host_name[TEMP_BUF_MAX_LEN] = {0};

    pid_t pid;
    int fd[2];

    //无名管道创建
    if(pipe(fd) < 0){
        LOG_ERROR << "pipe error";
        ret = -1;
        goto END;
    }

    //进程创建
    pid = fork();
    if(pid == 0){
        //子进程
        close(fd[0]);
        //重定向子进程标准输出到管道写端
        dup2(fd[1], STDOUT_FILENO);
        execlp("fdfs_file_info", "fdfs_file_info", s_dfs_path_client.c_str(), fileid, NULL);

        //执行失败
        LOG_ERROR << "execlp fdfs_file_info error";
        close(fd[1]);
    }
    else{
        //父进程
        LOG_INFO << "pid: " << pid;
        close(fd[1]);

        size_t n = read(fd[0], fdfs_file_stat_buf, TEMP_BUF_MAX_LEN);
        if(n <= 0){
            LOG_ERROR << "read error or no data";
            ret = -1;
            goto END;
        }

        wait(NULL);
        close(fd[0]);

        p = strstr(fdfs_file_stat_buf, "source ip address: ");
        if (!p) {
            LOG_ERROR << "failed to find 'source ip address:' in output: " << fdfs_file_stat_buf;
            ret = -1;
            goto END;
        }

        q = p + strlen("source ip address: ");

        k = strstr(q, "\n");
        if (!k) {
            LOG_ERROR << "failed to find newline after host string";
            ret = -1;
            goto END;
        }

        strncpy(fdfs_file_host_name, q, k-q);
        fdfs_file_host_name[k - q] = '\0';

    
        LOG_INFO << "host_name: " << s_storage_web_server_ip << ", fdfs_file_host_name: " <<  fdfs_file_host_name;

        // storage_web_server服务器的端口
        strcat(fdfs_file_url, "http://");
        strcat(fdfs_file_url, s_storage_web_server_ip.c_str());
        // strcat(fdfs_file_url, ":");
        // strcat(fdfs_file_url, s_storage_web_server_port.c_str());
        strcat(fdfs_file_url, "/");
        strcat(fdfs_file_url, fileid);

        LOG_INFO << "fdfs_file_url:" <<  fdfs_file_url;

    }

END:
    return ret;
}

int storeFileinfo(CDBConn *db_conn, CacheConn *cache_conn, char *user,
                  char *filename, char *md5, long size, char *fileid,
                  const char *fdfs_file_url
){
    int ret = 0;
    time_t now;
    char create_time[TIME_STRING_LEN] = {0};
    char suffix[SUFFIX_LEN] = {0};
    char sql_cmd[SQL_MAX_LEN] = {0};

    GetFileSuffix(filename, suffix);

    sprintf(sql_cmd, "insert into file_info (md5, file_id, url, size, type, count)"
                     "values ('%s', '%s', '%s', '%ld', '%s', '%d')",
                     md5, fileid, fdfs_file_url, size, suffix, 1);
    if(!db_conn->ExecuteCreate(sql_cmd)){
        LOG_ERROR << sql_cmd << " 操作失败";
        ret = -1;
        goto END;
    }

    now = time(NULL);
    strftime(create_time, TIME_STRING_LEN - 1, "%Y-%m-%d %H:%M:%S", localtime(&now));

    sprintf(sql_cmd,
            "insert into user_file_list(user, md5, create_time, file_name, "
            "shared_status, pv) values ('%s', '%s', '%s', '%s', %d, %d)",
            user, md5, create_time, filename, 0, 0);
    // LOG_INFO << "执行: " <<  sql_cmd;
    if (!db_conn->ExecuteCreate(sql_cmd)) {
        LOG_ERROR << sql_cmd << " 操作失败";
        ret = -1;
        goto END;
    }

    // // 询用户文件数量+1      web热点 大明星  微博存在缓存里面。
    // if (CacheIncrCount(cache_conn, string(user)) < 0) {
    //     LOG_ERROR << " CacheIncrCount 操作失败";
    // }
   

END:
    return ret;
}

int uploadFileToFastDfs(char* file_path, char* fileid){

    if(s_dfs_path_client.empty()){
        LOG_ERROR << "s_dfs_path_client is empty";
        return -1;
    }

    pid_t pid;
    int fd[2];
    //父进程读，子进程写（无名管道）
    if(pipe(fd) < 0){
        LOG_ERROR << "pipe error";
        return -1;
    }

    //创建进程
    pid = fork();
    if(pid < 0){
        LOG_ERROR << "fork error";
        return -1;
    }

    
    if(pid == 0){
        //子进程

        //关闭读端
        close(fd[0]);
        //重定向子进程标准输出到子进程管道的写端
        dup2(fd[1], STDOUT_FILENO);
        //执行上传命令
        execlp("fdfs_upload_file", "fdfs_upload_file", s_dfs_path_client.c_str(), file_path, NULL);
        //没有结束，错误
        LOG_ERROR << "execlp fdfs_upload_file error";
        close(fd[1]);
    }
    else{       
        //父进程

        //关闭写端
        close(fd[1]);
        //读数据
        size_t n = read(fd[0], fileid, TEMP_BUF_MAX_LEN);
        if(n <= 0){
            LOG_ERROR << "read error or no data";
            return -1;
        }

        LOG_INFO << "fileid1: " << fileid;
        TrimSpace(fileid);
        
        if(strlen(fileid) == 0){
            LOG_ERROR << "upload failed";
            return -1;
        }
        LOG_INFO << "fileid2: " << fileid;

        //回收子进程
        wait(NULL);
        close(fd[0]);
    }

    return 0;

}

//1.解析nginx临时文件的信息  2.将临时文件上传到fastDFS  3.删除nginx临时文件 4.获取storage的host
//5.上传文件信息到fastDFS 6.返回结果
int ApiUpload(string &post_data, string &resp_json){
    //准备保存信息
    char suffix[SUFFIX_LEN] = {0};
    char fileid[TEMP_BUF_MAX_LEN] = {0};
    char fdfs_file_url[FILE_URL_LEN] = {0};
    int ret = 0;
    char boundary[TEMP_BUF_MAX_LEN] = {0};
    char file_name[128] = {0};
    char file_content_type[128] = {0};
    char file_path[128] = {0};
    char new_file_path[128] = {0};
    char file_md5[128] = {0};
    char file_size[32] = {0};
    long long_file_size = 0;
    char user[32] = {0};

    //分段信息，保存文件信息
    char *begin = (char*)post_data.c_str();
    char* p1;
    char* p2;

    //返回结果
    Json::Value value;
    //获取数据库连接
    CDBManager* db_manager = CDBManager::getInstance();
    CDBConn* db_conn = db_manager->GetDBConn("tuchuang_master");
    AUTO_REL_DBCONN(db_manager, db_conn);

    //分割解析
    //查找分界线
    p1 = strstr(begin, "\r\n");
    if(p1 == NULL){
        LOG_ERROR << "wrong no boundary!";
        ret = -1;
        goto END;
    }
    //拷贝分界线
    strncpy(boundary, begin, p1 - begin);
    boundary[p1 - begin] = '\0';
    LOG_INFO << "boundary: " << boundary;

    //查找file_name
    begin = p1 + 2;
    p2 = strstr(begin, "name=\"file_name\"");
    if(!p2){
        LOG_ERROR << "wrong no file_name!";
        ret = -1;
        goto END;
    }
    p2 = strstr(begin, "\r\n");
    p2 += 4;
    begin = p2;
    p2 = strstr(begin, "\r\n");
    strncpy(file_name, begin, p2 - begin);
    LOG_INFO << "file_name: " << file_name;

    //查找file_content_type
    begin = p2 + 2;
    p2 = strstr(begin, "name=\"file_content_type\"");
    if(!p2){
        LOG_ERROR << "wrong no file_content_type!";
        ret = -1;
        goto END;
    }
    begin = p2;
    p2 = strstr(begin, "\r\n");
    p2 += 4;
    begin = p2;
    p2 = strstr(begin, "\r\n");
    strncpy(file_content_type, begin, p2 - begin);
    LOG_INFO << "file_content_type: " << file_content_type;

     // 查找文件file_path
    begin = p2 + 2;
    p2 = strstr(begin, "name=\"file_path\""); //
    if (!p2) {
        LOG_ERROR << "wrong no file_path!";
        ret = -1;
        goto END;
    }
    p2 = strstr(p2, "\r\n");
    p2 += 4;
    begin = p2;
    p2 = strstr(begin, "\r\n");
    strncpy(file_path, begin, p2 - begin);
    LOG_INFO << "file_path: " <<  file_path;

    // 查找文件file_md5
    begin = p2 + 2;
    p2 = strstr(begin, "name=\"file_md5\""); //
    if (!p2) {
        LOG_ERROR << "wrong no file_md5!";
        ret = -1;
        goto END;
    }
    p2 = strstr(p2, "\r\n");
    p2 += 4;
    begin = p2;
    p2 = strstr(begin, "\r\n");
    strncpy(file_md5, begin, p2 - begin);
    LOG_INFO << "file_md5: " <<  file_md5;

    // 查找文件file_size
    begin = p2 + 2;
    p2 = strstr(begin, "name=\"file_size\""); //
    if (!p2) {
        LOG_ERROR << "wrong no file_size!";
        ret = -1;
        goto END;
    }
    p2 = strstr(p2, "\r\n");
    p2 += 4;
    begin = p2;
    p2 = strstr(begin, "\r\n");
    strncpy(file_size, begin, p2 - begin);
    LOG_INFO << "file_size: " <<  file_size;
    long_file_size = strtol(file_size, NULL, 10); //字符串转long

    // 查找user
    begin = p2 + 2;
    p2 = strstr(begin, "name=\"user\""); //
    if (!p2) {
        LOG_ERROR << "wrong no user!";
        ret = -1;
        goto END;
    }
    p2 = strstr(p2, "\r\n");
    p2 += 4;
    begin = p2;
    p2 = strstr(begin, "\r\n");
    strncpy(user, begin, p2 - begin);
    LOG_INFO << "user: " <<  user;

    //把临时文件拷贝到fastDFS
    //重命名临时文件
    GetFileSuffix(file_name, suffix);
    strcat(new_file_path, file_path);
    strcat(new_file_path, ".");
    strcat(new_file_path, suffix);
    ret = rename(file_path, new_file_path);

    if(ret < 0){
        LOG_ERROR << file_path << " to" << new_file_path << " failed";
        ret = -1;
        goto END;
    }

    //拷贝
    LOG_INFO << "uploadFileToFastDfs, file_name: " << file_name << ", new_file_path: " << new_file_path;
    ret = uploadFileToFastDfs(new_file_path, fileid);
        //拷贝失败
    if(ret < 0){
        LOG_ERROR << "upload " << file_name << "To FastDFS failed, file_path: " << new_file_path; 

        ret = unlink(new_file_path);
        //删除临时文件失败
        if(ret != 0){
            LOG_ERROR << "unlink: " << new_file_path << " failed";
        }
        ret = -1;
        goto END;
    }

    //删除nginx临时文件
    LOG_INFO << "unlink: " << new_file_path;
    ret = unlink(new_file_path);
    if(ret < 0){
        LOG_ERROR << "unlink: " << new_file_path << " failed";
        ret = -1;
        goto END;
    }

    //得到文件所存放的storage的host_name
    LOG_INFO << "getFullurlByFileid, fileid: " << fileid;
    if(getFullUrlByFileid(fileid, fdfs_file_url) < 0){
        LOG_ERROR << "getFullurlByFileid failed";
        ret = -1;
        goto END;
    }

    //上传文件的FastDFS信息到mysql
    LOG_INFO << "storeFileinfo, origin url: " << fdfs_file_url ;// << " -> short url: " <<  short_url;
     //把文件写入file_info
    if(storeFileinfo(db_conn, NULL, user, file_name, file_md5,
                     long_file_size, fileid, fdfs_file_url) < 0){
        LOG_ERROR << "storeFileinfo failed ";
        ret = -1;
        goto END;
    }

    ret = 0;
    value["code"] = 0;
    resp_json = value.toStyledString();
    return 0;

END:
    value["code"] = 1;
    resp_json = value.toStyledString(); // json序列化

    return -1;
}

int ApiUploadInit(const char *dfs_path_client, 
                    const char *storage_web_server_ip, const char *storage_web_server_port, 
                  const char *shorturl_server_address, const char *access_token) {
    s_dfs_path_client = dfs_path_client;
    s_storage_web_server_ip = storage_web_server_ip;
    s_storage_web_server_port = storage_web_server_port;
    s_shorturl_server_address = shorturl_server_address;
    s_shorturl_server_access_token = access_token;
    return 0;
}

