/*
 * @Description: 
 * @Autor: Weihang Shen
 * @Date: 2022-01-26 00:12:21
 * @LastEditors: Weihang Shen
 * @LastEditTime: 2022-01-26 22:43:04
 */
#ifndef SERVER_H
#define SERVER_H

#include <stdlib.h>

#include "lib/cJSON.h"
#include "lib/open62541.h"

class Server
{
private:
    UA_UInt16 port;
    UA_Boolean running;
    UA_Server *uaServer;
    cJSON *configRoot;

    UA_StatusCode loadConfig();
    UA_StatusCode addNode(cJSON *configNode, UA_NodeId paren);

public:
    Server(char *config);
    void start();
    void stop();
};

#endif