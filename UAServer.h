/*
 * @Description: 
 * @Autor: Weihang Shen
 * @Date: 2022-01-26 00:12:21
 * @LastEditors: Weihang Shen
 * @LastEditTime: 2022-01-30 23:59:21
 */
#ifndef SERVER_H
#define SERVER_H

#include <open62541/server.h>

UA_StatusCode Server_init(char *config);
UA_StatusCode Server_start();
UA_StatusCode Server_stop();

#endif