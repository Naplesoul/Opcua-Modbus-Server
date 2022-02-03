/*
 * @Description: 
 * @Autor: Weihang Shen
 * @Date: 2022-01-26 00:12:29
 * @LastEditors: Weihang Shen
 * @LastEditTime: 2022-02-04 02:07:54
 */

#include <stdlib.h>

#include <cjson/cJSON.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include "libmodbus/src/modbus.h"

#include "UAServer.h"
#include "utils/HashMap.h"

#define CONFIG_ERROR -1
#define CONFIG_OK 0
typedef struct
{
    modbus_t *modbus_rtu;
    uint32_t mb_machine_address;
    uint32_t mb_register_address;

} Modbus_Access;

enum MB_Register_Type
{
    MB_COIL_STATUS, MB_INPUT_STATUS, MB_HOLDING_REGISTER, MB_INPUT_REGISTER
};

UA_StatusCode Server_addModbusRTU(cJSON *rtu_config);
UA_StatusCode Server_addModbusVariable(cJSON *var_config, modbus_t *device);

UA_UInt16 port = 4840;
UA_Boolean running = false;
cJSON *config_root = NULL;
UA_Server *ua_server = NULL;
HashMap *modbus_access_map = NULL;

UA_StatusCode Server_init(char *config)
{
    config_root = cJSON_Parse(config);
    modbus_access_map = Map_new();

    if (cJSON_HasObjectItem(config_root, "Port") == 0)
		return UA_STATUSCODE_BADDECODINGERROR;

    port = cJSON_GetObjectItem(config_root, "Port")->valueint;

    running = false;
    ua_server = UA_Server_new();
    UA_ServerConfig_setMinimal(UA_Server_getConfig(ua_server), port, NULL);

    if (cJSON_HasObjectItem(config_root, "AddressSpace") == 0)
		return UA_STATUSCODE_BADDECODINGERROR;
    
	cJSON *address_space = cJSON_GetObjectItem(config_root, "AddressSpace");
	int node_count = cJSON_GetArraySize(address_space);

	if (node_count <= 0)
		return UA_STATUSCODE_BADDECODINGERROR;
    
	for (int i = 0; i < node_count; ++i) {
        cJSON *config_node = cJSON_GetArrayItem(address_space, i);
        UA_StatusCode status_code = Server_addNode(config_node, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), NULL);
        cJSON_Delete(config_node);
		if (status_code != 0)
			return status_code;
	}
    
	cJSON_Delete(address_space);
	return 0;
}

UA_StatusCode Server_start()
{
    running = true;
    UA_StatusCode status_code = UA_Server_run(ua_server, &running);
    return status_code;
}

void Server_beforeRead(UA_Server *server,
			           const UA_NodeId *session_id, void *session_context,
                       const UA_NodeId *node_id, void *node_context,
                       const UA_NumericRange *range, const UA_DataValue *data)
{
    UA_Variant value;
    u_int16_t new_value;
    // TODO: read value from modbus reg
    Modbus_Access *access = (Modbus_Access *)Map_get(modbus_access_map, UA_NodeId_hash(node_id));
    modbus_read_registers(access->modbus_rtu, access->mb_register_address, 1, &new_value);
    UA_Double now = new_value;
    UA_Variant_setScalar(&value, &now, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server, *node_id, value);
}

void Server_afterWrite(UA_Server *server,
                       const UA_NodeId *session_id, void *session_context,
                       const UA_NodeId *node_id, void *node_context,
                       const UA_NumericRange *range, const UA_DataValue *data)
{
    double new_value = *(UA_Double *)(data->value.data);
    // TODO: write value to modbus reg
    Modbus_Access *access = (Modbus_Access *)Map_get(modbus_access_map, UA_NodeId_hash(node_id));
    modbus_write_register(access->modbus_rtu, access->mb_register_address, new_value);
    return;
}

UA_StatusCode Server_addModbusRTU(cJSON *rtu_config)
{
    int ns_index = cJSON_GetObjectItem(rtu_config, "NodeID_NamespaceIndex")->valueint;
    UA_NodeId this_node_id = UA_NODEID_STRING(ns_index, cJSON_GetObjectItem(rtu_config, "NodeID_ID")->valuestring);
    UA_NodeId ref_node_id = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);

    modbus_t *modbus_rtu = modbus_new_rtu(cJSON_GetObjectItem(rtu_config, "Device")->valuestring,
                                          cJSON_GetObjectItem(rtu_config, "Baud")->valueint,
                                          cJSON_GetObjectItem(rtu_config, "Parity")->valuestring[0],
                                          cJSON_GetObjectItem(rtu_config, "DataBits")->valueint,
                                          cJSON_GetObjectItem(rtu_config, "StopBits")->valueint);

    if (strcmp(cJSON_GetObjectItem(rtu_config, "SerialMode")->valuestring, "RS485") == 0) {
        modbus_rtu_set_serial_mode(modbus_rtu, MODBUS_RTU_RS485);
    } else if (strcmp(cJSON_GetObjectItem(rtu_config, "SerialMode")->valuestring, "RS232") == 0) {
        modbus_rtu_set_serial_mode(modbus_rtu, MODBUS_RTU_RS232);
    } else {
        return CONFIG_ERROR;
    }

    UA_ObjectAttributes obj_attr = UA_ObjectAttributes_default;
    obj_attr.description = UA_LOCALIZEDTEXT("en-US", cJSON_GetObjectItem(rtu_config, "Description")->valuestring);
    obj_attr.displayName = UA_LOCALIZEDTEXT("en-US", cJSON_GetObjectItem(rtu_config, "DisplayName")->valuestring);
    UA_Server_addObjectNode(ua_server, this_node_id,
                            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                            ref_node_id,
                            UA_QUALIFIEDNAME(1, cJSON_GetObjectItem(rtu_config, "QualifiedName")->valuestring),
                            UA_NODEID_NULL,
                            obj_attr, NULL, NULL);

    if (cJSON_HasObjectItem(rtu_config, "Variables")) {
        cJSON *variables = cJSON_GetObjectItem(rtu_config, "Variables");
        int var_count = cJSON_GetArraySize(variables);

        for (int i = 0; i < var_count; ++i) {
            cJSON *var_config = cJSON_GetArrayItem(variables, i);
            UA_StatusCode status_code = Server_addModbusVariable(var_config, modbus_rtu);
            cJSON_Delete(var_config);
            if (status_code < 0) {
                cJSON_Delete(variables);
                return status_code;
            }
        }
        cJSON_Delete(variables);
    }
    return CONFIG_OK;
}

UA_StatusCode Server_addModbusVariable(cJSON *var_config, modbus_t *device)
{
    int ns_index = cJSON_GetObjectItem(var_config, "NodeID_NamespaceIndex")->valueint;
    UA_NodeId this_node_id = UA_NODEID_STRING(ns_index, cJSON_GetObjectItem(var_config, "NodeID_ID")->valuestring);
    UA_NodeId ref_node_id = UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY);

    UA_VariableAttributes var_attr = UA_VariableAttributes_default;
    var_attr.description = UA_LOCALIZEDTEXT("en-US", cJSON_GetObjectItem(var_config, "Description")->valuestring);
    var_attr.displayName = UA_LOCALIZEDTEXT("en-US", cJSON_GetObjectItem(var_config, "DisplayName")->valuestring);
    
    char *variable_type = cJSON_GetObjectItem(var_config, "VariableType")->valuestring;
    if (strcmp(variable_type, "CoilStatus") == 0) {
        // TODO
    }
}

UA_StatusCode Server_addNode(cJSON *config_node, UA_NodeId parent_node_id, modbus_t *parent_rtu)
{
    int ns_index = cJSON_GetObjectItem(config_node, "NodeID_NamespaceIndex")->valueint;
    if (ns_index > (2 << 15) - 1) {
        // overflow
        return UA_STATUSCODE_BADOUTOFRANGE;
    }

    UA_NodeId this_node_id;
    if (strcmp(cJSON_GetObjectItem(config_node, "NodeID_IDType")->valuestring, "String") == 0) {
        this_node_id = UA_NODEID_STRING(ns_index, cJSON_GetObjectItem(config_node, "NodeID_ID")->valuestring);
    } else if (strcmp(cJSON_GetObjectItem(config_node, "NodeID_IDType")->valuestring, "Numeric") == 0) {
        this_node_id = UA_NODEID_NUMERIC(ns_index, cJSON_GetObjectItem(config_node, "NodeID_ID")->valueint);
    } else {
        return UA_STATUSCODE_BADOUTOFRANGE;
    }

    UA_NodeId ref_node_id;
    if (strcmp(cJSON_GetObjectItem(config_node, "ReferenceTypeId")->valuestring, "Organizes") == 0) {
        ref_node_id = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    } else if (strcmp(cJSON_GetObjectItem(config_node, "ReferenceTypeId")->valuestring, "HasComponent") == 0) {
        ref_node_id = UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT);
    } else if (strcmp(cJSON_GetObjectItem(config_node, "ReferenceTypeId")->valuestring, "HasProperty") == 0) {
        ref_node_id = UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY);
    } else {
        return UA_STATUSCODE_BADOUTOFRANGE;
    }

    if (strcmp(cJSON_GetObjectItem(config_node, "NodeClass")->valuestring, "Object") == 0) {
        if (strcmp(cJSON_GetObjectItem(config_node, "Communication")->valuestring, "Serial") == 0) {
            parent_rtu = modbus_new_rtu(cJSON_GetObjectItem(config_node, "Device")->valuestring,
                                        cJSON_GetObjectItem(config_node, "Baud")->valueint,
                                        cJSON_GetObjectItem(config_node, "Parity")->valuestring[0],
                                        cJSON_GetObjectItem(config_node, "DataBits")->valueint,
                                        cJSON_GetObjectItem(config_node, "StopBits")->valueint);
            modbus_set_slave(parent_rtu, cJSON_GetObjectItem(config_node, "MB_MachineAddress")->valueint);
            if (strcmp(cJSON_GetObjectItem(config_node, "SerialMode")->valuestring, "RS485") == 0) {
                modbus_rtu_set_serial_mode(parent_rtu, MODBUS_RTU_RS485);
            }
            if (modbus_connect(parent_rtu) < 0) {
                return -1;
            }
        } else {
            printf("Only support serial devices now...\n");
            return UA_STATUSCODE_BADDECODINGERROR;
        }
        UA_ObjectAttributes obj_attr = UA_ObjectAttributes_default;
        obj_attr.description = UA_LOCALIZEDTEXT("en-US", cJSON_GetObjectItem(config_node, "Description")->valuestring);
        obj_attr.displayName = UA_LOCALIZEDTEXT("en-US", cJSON_GetObjectItem(config_node, "DisplayName")->valuestring);
        UA_Server_addObjectNode(ua_server,
                                this_node_id,
                                parent_node_id,
                                ref_node_id,
                                UA_QUALIFIEDNAME(1, cJSON_GetObjectItem(config_node, "QualifiedName")->valuestring),
                                UA_NODEID_NULL,
                                obj_attr, NULL, NULL);

    } else if (strcmp(cJSON_GetObjectItem(config_node, "NodeClass")->valuestring, "Variable") == 0) {
        UA_VariableAttributes var_attr = UA_VariableAttributes_default;
        var_attr.description = UA_LOCALIZEDTEXT("en-US", cJSON_GetObjectItem(config_node, "Description")->valuestring);
        var_attr.displayName = UA_LOCALIZEDTEXT("en-US", cJSON_GetObjectItem(config_node, "DisplayName")->valuestring);

        if (strcmp(cJSON_GetObjectItem(config_node, "DataType")->valuestring, "Double") == 0) {
            UA_Double *temp = (UA_Double *)malloc(sizeof(UA_Double));
			var_attr.dataType = UA_NODEID_NUMERIC(0, UA_NS0ID_DOUBLE);
			*temp = cJSON_GetObjectItem(config_node, "InitialValue")->valuedouble;
			UA_Variant_setScalar(&var_attr.value, temp, &UA_TYPES[UA_TYPES_DOUBLE]);
        } else if (strcmp(cJSON_GetObjectItem(config_node, "DataType")->valuestring, "Int16") == 0) {
            UA_UInt16 tmp = cJSON_GetObjectItem(config_node, "InitialValue")->valueint;
			var_attr.dataType = UA_NODEID_NUMERIC(0, UA_NS0ID_UINT16);
			UA_Variant_setScalar(&var_attr.value, &tmp, &UA_TYPES[UA_TYPES_UINT16]);
        } else {
            return UA_STATUSCODE_BADOUTOFRANGE;
        }
        UA_Server_addVariableNode(ua_server,
                                  this_node_id,
                                  parent_node_id,
                                  ref_node_id,
                                  UA_QUALIFIEDNAME(1, cJSON_GetObjectItem(config_node, "QualifiedName")->valuestring),
                                  UA_NODEID_NULL,
                                  var_attr, NULL, NULL);
        
        Modbus_Access *access = (Modbus_Access *)malloc(sizeof(Modbus_Access));
        access->mb_machine_address = cJSON_GetObjectItem(config_node, "MB_MachineAddress")->valueint;
        access->mb_register_address = cJSON_GetObjectItem(config_node, "MB_RegisterAddress")->valueint;
        access->modbus_rtu = parent_rtu;

        Map_put(modbus_access_map, UA_NodeId_hash(&this_node_id), access);

        // binding read & write callback functions
        UA_ValueCallback callback;
	    callback.onRead = Server_beforeRead;
	    callback.onWrite = Server_afterWrite;
	    UA_Server_setVariableNode_valueCallback(ua_server, this_node_id, callback);

    } else {
        return UA_STATUSCODE_BADOUTOFRANGE;
    }

    if (cJSON_HasObjectItem(config_node, "ChildNode")) {
        cJSON *child_nodes = cJSON_GetObjectItem(config_node, "ChildNode");
        int node_count = cJSON_GetArraySize(child_nodes);

        for (int i = 0; i < node_count; ++i) {
            cJSON *child_config_node = cJSON_GetArrayItem(child_nodes, i);
            UA_StatusCode status_code = Server_addNode(child_config_node, this_node_id, parent_rtu);
            cJSON_Delete(child_config_node);
            if (status_code < 0) {
                return status_code;
            }
        }

        cJSON_Delete(child_nodes);
    }

    return 0;
}