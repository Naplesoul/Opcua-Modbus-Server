/*
 * @Description: 
 * @Autor: Weihang Shen
 * @Date: 2022-01-26 00:12:21
 * @LastEditors: Weihang Shen
 * @LastEditTime: 2022-01-28 23:52:53
 */
#ifndef SERVER_H
#define SERVER_H

#include <stdlib.h>

#include <cjson/cJSON.h>
#include <open62541/server.h>

typedef struct
{
    UA_UInt16 port;
    UA_Boolean running;
    cJSON *config_root;

    UA_Server *ua_server;
} OPCUA_Server;

UA_StatusCode init_server(OPCUA_Server *server, UA_UInt16 port, char *config);
UA_StatusCode add_node(OPCUA_Server *server, cJSON *config_node, UA_NodeId parent_node_id);

UA_StatusCode start_server(OPCUA_Server *server);
UA_StatusCode stop_server(OPCUA_Server *server);

#endif