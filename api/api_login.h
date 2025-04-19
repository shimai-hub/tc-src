#ifndef __API_LOGIN_H__
#define __API_LOGIN_H__

#include <string>
using namespace std;


//将用户信息与注册信息对比，并返回结果
int ApiLoginUser(string& post_data, string& resp_json);


#endif