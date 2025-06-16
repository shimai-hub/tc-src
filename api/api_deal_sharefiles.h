#ifndef _API_DEALSHAREFILE_H_
#define _API_DEALSHAREFILE_H_
#include "api_common.h"

// 1.转存共享文件（也上传到自己的文件库）{获取共享文件信息，写入当前用户文件信息相关表单} 
// 2.取消共享 {从共享文件相关表单删除该文件数据，从redis中删除该文件信息，修改用户文件共享状态} 
// 3.更新共享文件下载量 {获取该共享文件下载量，+1后更新进去（mysql && redis）}

// redis只存储共享文件信息
// Zset - FILE_PUBLIC_ZSET - {pv-filename}
// Hash - FILE_NAME_HASH - {fileid, filename}
int ApiDealsharefile(string &url, string &post_data, string &str_json);
#endif // ! _API_DEALSHAREFILE_H_
