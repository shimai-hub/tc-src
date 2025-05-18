#ifndef _APIUPLOAD_H_
#define _APIUPLOAD_H_
#include "api_common.h"

int ApiUpload(string &post_data, string &resp_json);
int ApiUploadInit(const char *dfs_path_client, 
                    const char *storage_web_server_ip, const char *storage_web_server_port, 
                  const char *shorturl_server_address, const char *access_token);

#endif // !__UPLOAD_H_
