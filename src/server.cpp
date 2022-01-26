/*
 * @Description: 
 * @Autor: Weihang Shen
 * @Date: 2022-01-26 00:12:29
 * @LastEditors: Weihang Shen
 * @LastEditTime: 2022-01-26 23:29:40
 */
#include "lib/open62541.h"
#include "server.hpp"
#include "log.h"

/**
 * @description: 
 * @param {char} *config: c style string which constains the config json
 * @return none
 * @author: Weihang Shen
 */
Server::Server(char *config)
{
    this->port = 0;
    this->running = false;
    this->configRoot = cJSON_Parse(config);
}

/**
 * @description: 
 * @param none
 * @return status code, negative when error
 * @author: Weihang Shen
 */
UA_StatusCode Server::loadConfig()
{
	if (cJSON_HasObjectItem(this->configRoot, "AddressSpace") == 0)
		return UA_STATUSCODE_BADDECODINGERROR;
    
	cJSON *addressSpace = cJSON_GetObjectItem(this->configRoot, "AddressSpace");
	int nodeCount = cJSON_GetArraySize(addressSpace);

	if (nodeCount <= 0)
		return UA_STATUSCODE_BADDECODINGERROR;
    
	for (int i = 0; i < nodeCount; ++i) {
        cJSON *configNode = cJSON_GetArrayItem(addressSpace, i);
		UA_StatusCode err = this->addNode(configNode, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER));
        cJSON_Delete(configNode);
		if (err != 0)
			return err;
	}
    
	cJSON_Delete(addressSpace);
	return 0;
}

UA_StatusCode Server::addNode(cJSON *configNode, UA_NodeId parentNodeId)
{
    int nsIndex = cJSON_GetObjectItem(configNode, "NodeID_NamespaceIndex")->valueint;
    if (nsIndex > (2 << 15) - 1) {
        // overflow
        return UA_STATUSCODE_BADOUTOFRANGE;
    }

    UA_NodeId nodeId;
    if (strcmp(cJSON_GetObjectItem(configNode, "NodeID_IDType")->valuestring, "String") == 0) {
        nodeId = UA_NODEID_STRING(nsIndex, cJSON_GetObjectItem(configNode, "NodeID_ID")->valuestring);
    } else if (strcmp(cJSON_GetObjectItem(configNode, "NodeID_IDType")->valuestring, "Numeric") == 0) {
        nodeId = UA_NODEID_NUMERIC(nsIndex, cJSON_GetObjectItem(configNode, "NodeID_ID")->valueint);
    } else {
        assert(0);
    }

    UA_NodeId refNodeID;
    if (strcmp(cJSON_GetObjectItem(configNode, "ReferenceTypeId")->valuestring, "Organizes") == 0) {
        refNodeID = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    } else if (strcmp(cJSON_GetObjectItem(configNode, "ReferenceTypeId")->valuestring, "HasComponent") == 0) {
        refNodeID = UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT);
    } else if (strcmp(cJSON_GetObjectItem(configNode, "ReferenceTypeId")->valuestring, "HasProperty") == 0) {
        refNodeID = UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY);
    } else {
        assert(0);
    }

    if (strcmp(cJSON_GetObjectItem(configNode, "NodeClass")->valuestring, "Object") == 0) {
        UA_ObjectAttributes objAttr = UA_ObjectAttributes_default;
        objAttr.description = UA_LOCALIZEDTEXT("en-US", cJSON_GetObjectItem(configNode, "Description")->valuestring);
        objAttr.displayName = UA_LOCALIZEDTEXT("en-US", cJSON_GetObjectItem(configNode, "DisplayName")->valuestring);
        UA_Server_addObjectNode(this->uaServer,
                                nodeId,
                                parentNodeId,
                                refNodeID,
                                UA_QUALIFIEDNAME(1, cJSON_GetObjectItem(configNode, "QualifiedName")->valuestring),
                                UA_NODEID_NULL,
                                objAttr, NULL, NULL);

    } else if (strcmp(cJSON_GetObjectItem(configNode, "NodeClass")->valuestring, "Variable") == 0) {
        UA_VariableAttributes varAttr = UA_VariableAttributes_default;
        varAttr.description = UA_LOCALIZEDTEXT("en-US", cJSON_GetObjectItem(configNode, "Description")->valuestring);
        varAttr.displayName = UA_LOCALIZEDTEXT("en-US", cJSON_GetObjectItem(configNode, "DisplayName")->valuestring);

        if (strcmp(cJSON_GetObjectItem(configNode, "DataType")->valuestring, "Double") == 0) {
            UA_Double *temp = (UA_Double *)malloc(sizeof(UA_Double));
			varAttr.dataType = UA_NODEID_NUMERIC(0, UA_NS0ID_DOUBLE);
			*temp = cJSON_GetObjectItem(configNode, "InitialValue")->valuedouble;
			UA_Variant_setScalar(&varAttr.value, temp, &UA_TYPES[UA_TYPES_DOUBLE]);
        } else {
            assert(0);
        }
        UA_Server_addVariableNode(this->uaServer,
                                  nodeId,
                                  parentNodeId,
                                  refNodeID,
                                  UA_QUALIFIEDNAME(1, cJSON_GetObjectItem(configNode, "QualifiedName")->valuestring),
                                  UA_NODEID_NULL,
                                  varAttr, NULL, NULL);
        
    } else {
        assert(0);
    }

}

void Server::start()
{
    this->uaServer = UA_Server_new();
    UA_ServerConfig_setMinimal(UA_Server_getConfig(this->uaServer), this->port, NULL);
    this->running = true;
    this->loadConfig();
    UA_StatusCode statusCode = UA_Server_run(this->uaServer, &this->running);
    LOG_INFO("Server stopped")
}