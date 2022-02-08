/*
 * @Description: 
 * @Autor: Weihang Shen
 * @Date: 2022-02-09 00:48:52
 * @LastEditors: Weihang Shen
 * @LastEditTime: 2022-02-09 01:58:48
 */

#include <open62541/server.h>

UA_StatusCode
UA_Server_editNode(UA_Server *server, UA_Session *session,
                   const UA_NodeId *nodeId, UA_EditNodeCallback callback,
                   void *data) {
#ifndef UA_ENABLE_IMMUTABLE_NODES
    /* Get the node and process it in-situ */
    const UA_Node *node = UA_Nodestore_getNode(server->nsCtx, nodeId);
    if(!node)
        return UA_STATUSCODE_BADNODEIDUNKNOWN;
    UA_StatusCode retval = callback(server, session, (UA_Node*)(uintptr_t)node, data);
    UA_Nodestore_releaseNode(server->nsCtx, node);
    return retval;
#else
    UA_StatusCode retval;
    do {
        /* Get an editable copy of the node */
        UA_Node *node;
        retval = UA_Nodestore_getNodeCopy(server->nsCtx, nodeId, &node);
        if(retval != UA_STATUSCODE_GOOD)
            return retval;

        /* Run the operation on the copy */
        retval = callback(server, session, node, data);
        if(retval != UA_STATUSCODE_GOOD) {
            UA_Nodestore_deleteNode(server->nsCtx, node);
            return retval;
        }

        /* Replace the node */
        retval = UA_Nodestore_replaceNode(server->nsCtx, node);
    } while(retval != UA_STATUSCODE_GOOD);
    return retval;
#endif
}

UA_StatusCode
UA_Server_write(UA_Server *server, const UA_WriteValue *value) {
    return UA_Server_editNode(server, &server->adminSession, &value->nodeId,
                              (UA_EditNodeCallback)copyAttributeIntoNode,
                              /* casting away const qualifier because callback uses const anyway */
                              (UA_WriteValue *)(uintptr_t)value);
}

UA_StatusCode
__UA_Server_write_without_callback(UA_Server *server, const UA_NodeId *nodeId,
                                   const UA_AttributeId attributeId,
                                   const UA_DataType *attr_type,
                                   const void *attr)
{
    UA_WriteValue wvalue;
    UA_WriteValue_init(&wvalue);
    wvalue.nodeId = *nodeId;
    wvalue.attributeId = attributeId;
    wvalue.value.hasValue = true;
    if(attr_type != &UA_TYPES[UA_TYPES_VARIANT]) {
        /* hacked cast. the target WriteValue is used as const anyway */
        UA_Variant_setScalar(&wvalue.value.value,
                             (void*)(uintptr_t)attr, attr_type);
    } else {
        wvalue.value.value = *(const UA_Variant*)attr;
    }
    return UA_Server_write(server, &wvalue);
}

static UA_INLINE UA_StatusCode
UA_Server_writeValueWithoutCallback(UA_Server *server, const UA_NodeId nodeId, const UA_Variant value)
{
    return __UA_Server_write_without_callback(server, &nodeId, UA_ATTRIBUTEID_VALUE, &UA_TYPES[UA_TYPES_VARIANT], &value);
}