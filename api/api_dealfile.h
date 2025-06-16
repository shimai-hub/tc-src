#ifndef __API_DEALFILE_H__
#define __API_LOG__API_DEALFILE_H__IN_H__

#include "api_common.h"
using namespace std;


//1.分享文件，将文件信息写入share_file_list
//2.删除文件
//3.下载文件
int ApiDealfile(string& url, string& post_data, string& resp_json);


#endif