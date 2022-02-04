/*
 * @Description: 
 * @Autor: Weihang Shen
 * @Date: 2022-01-26 00:12:29
 * @LastEditors: Weihang Shen
 * @LastEditTime: 2022-02-05 01:14:28
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

typedef enum
{
    MB_COIL_STATUS, MB_INPUT_STATUS, MB_HOLDING_REGISTER, MB_INPUT_REGISTER
} MB_Variable_Type;

typedef enum
{
    MB_BOOLEAN, MB_FLOAT_ABCD, MB_FLOAT_BADC, MB_FLOAT_CDAB, MB_FLOAT_DCBA, MB_UINT16, MB_UINT32
} MB_Data_Type;

typedef struct
{
    modbus_t *device;
    MB_Variable_Type var_type;
    MB_Data_Type data_type;
    uint32_t mb_machine_address;
    uint32_t mb_register_address;

} Modbus_Access;

UA_StatusCode Server_init(char *config);
UA_StatusCode Server_start();
void Server_beforeRead(UA_Server *server,
			           const UA_NodeId *session_id, void *session_context,
                       const UA_NodeId *node_id, void *node_context,
                       const UA_NumericRange *range, const UA_DataValue *data);
void Server_afterWrite(UA_Server *server,
                       const UA_NodeId *session_id, void *session_context,
                       const UA_NodeId *node_id, void *node_context,
                       const UA_NumericRange *range, const UA_DataValue *data);
UA_StatusCode Server_addModbusRTU(cJSON *rtu_config);
UA_StatusCode Server_addModbusVariable(cJSON *var_config, UA_NodeId parent_node_id, modbus_t *device);
UA_StatusCode Server_setVariableType(cJSON *var_config, Modbus_Access *access, UA_VariableAttributes *var_attr);
UA_StatusCode Server_setVariableTypeAndInitalValue(cJSON *var_config, Modbus_Access *access, UA_VariableAttributes *var_attr);

/*####################################################################*/

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
		return CONFIG_ERROR;

    port = cJSON_GetObjectItem(config_root, "Port")->valueint;

    running = false;
    ua_server = UA_Server_new();
    UA_ServerConfig_setMinimal(UA_Server_getConfig(ua_server), port, NULL);

    if (cJSON_HasObjectItem(config_root, "ModbusRTUs") == 0)
		return CONFIG_ERROR;
    
	cJSON *modbus_rtus = cJSON_GetObjectItem(config_root, "ModbusRTUs");
	int rtu_count = cJSON_GetArraySize(modbus_rtus);

	if (rtu_count <= 0)
		return CONFIG_ERROR;
    
	for (int i = 0; i < rtu_count; ++i) {
        cJSON *rtu_config = cJSON_GetArrayItem(modbus_rtus, i);
        UA_StatusCode status_code = Server_addNode(rtu_config, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), NULL);
        cJSON_Delete(rtu_config);
		if (status_code != 0) return status_code;
	}
    
	cJSON_Delete(modbus_rtus);
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
    modbus_read_registers(access->device, access->mb_register_address, 1, &new_value);
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
    modbus_write_register(access->device, access->mb_register_address, new_value);
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
            UA_StatusCode status_code = Server_addModbusVariable(var_config, this_node_id, modbus_rtu);
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

UA_StatusCode Server_addModbusVariable(cJSON *var_config, UA_NodeId parent_node_id, modbus_t *device)
{
    int ns_index = cJSON_GetObjectItem(var_config, "NodeID_NamespaceIndex")->valueint;
    UA_NodeId this_node_id = UA_NODEID_STRING(ns_index, cJSON_GetObjectItem(var_config, "NodeID_ID")->valuestring);
    UA_NodeId ref_node_id = UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY);

    UA_VariableAttributes var_attr = UA_VariableAttributes_default;
    var_attr.description = UA_LOCALIZEDTEXT("en-US", cJSON_GetObjectItem(var_config, "Description")->valuestring);
    var_attr.displayName = UA_LOCALIZEDTEXT("en-US", cJSON_GetObjectItem(var_config, "DisplayName")->valuestring);
    
    Modbus_Access *access = (Modbus_Access *)malloc(sizeof(Modbus_Access));
    access->device = device;
    access->mb_machine_address = cJSON_GetObjectItem(var_config, "MB_MachineAddress")->valueint;
    access->mb_register_address = cJSON_GetObjectItem(var_config, "MB_RegisterAddress")->valueint;
    char *variable_type = cJSON_GetObjectItem(var_config, "VariableType")->valuestring;

    if (strcmp(variable_type, "CoilStatus") == 0) {
        access->var_type = MB_COIL_STATUS;
        access->data_type = MB_BOOLEAN;
        var_attr.dataType = UA_NODEID_NUMERIC(0, UA_NS0ID_BOOLEAN);
        if (cJSON_HasObjectItem(var_config, "InitialValue")) {
            UA_Boolean init_value = cJSON_GetObjectItem(var_config, "InitialValue")->valueint;
            UA_Variant_setScalar(&var_attr.value, &init_value, &UA_TYPES[UA_TYPES_BOOLEAN]);
        }
    } else if (strcmp(variable_type, "InputStatus") == 0) {
        access->var_type = MB_INPUT_STATUS;
        access->data_type = MB_BOOLEAN;
        var_attr.dataType = UA_NODEID_NUMERIC(0, UA_NS0ID_BOOLEAN);
        // TODO: Set variable READ_ONLY
        // var_attr.userWriteMask = ?;
    } else if (strcmp(variable_type, "HoldingRegister") == 0) {
        access->var_type = MB_HOLDING_REGISTER;
        UA_StatusCode status_code = Server_setVariableTypeAndInitalValue(var_config, access, &var_attr);
        if (status_code != CONFIG_OK) return status_code;
    } else if (strcmp(variable_type, "InputRegister") == 0) {
        access->var_type = MB_INPUT_REGISTER;
        UA_StatusCode status_code = Server_setVariableType(var_config, access, &var_attr);
        if (status_code != CONFIG_OK) return status_code;
        // TODO: Set variable READ_ONLY
        // var_attr.userWriteMask = ?;
    } else {
        return CONFIG_ERROR;
    }

    UA_Server_addVariableNode(ua_server,
                              this_node_id,
                              parent_node_id,
                              ref_node_id,
                              UA_QUALIFIEDNAME(1, cJSON_GetObjectItem(var_config, "QualifiedName")->valuestring),
                              UA_NODEID_NULL,
                              var_attr, NULL, NULL);
    
    Map_put(modbus_access_map, UA_NodeId_hash(&this_node_id), access);
    // binding read & write callback functions
    UA_ValueCallback callback;
	callback.onRead = Server_beforeRead;
	callback.onWrite = Server_afterWrite;
	UA_Server_setVariableNode_valueCallback(ua_server, this_node_id, callback);

    return CONFIG_OK;
}

UA_StatusCode Server_setVariableType(cJSON *var_config, Modbus_Access *access, UA_VariableAttributes *var_attr)
{
    char *data_type = cJSON_GetObjectItem(var_config, "DataType")->valuestring;
    if (strcmp(data_type, "FLOAT") == 0) {
        var_attr->dataType = UA_NODEID_NUMERIC(0, UA_NS0ID_FLOAT);

        char *byte_order = cJSON_GetObjectItem(var_config, "ByteOrder")->valuestring;
        if (strcmp(byte_order, "abcd") == 0) access->data_type = MB_FLOAT_ABCD;
        else if (strcmp(byte_order, "badc") == 0) access->data_type = MB_FLOAT_BADC;
        else if (strcmp(byte_order, "cdab") == 0) access->data_type = MB_FLOAT_CDAB;
        else if (strcmp(byte_order, "dcba") == 0) access->data_type = MB_FLOAT_DCBA;
        else return CONFIG_ERROR;

    } else if (strcmp(data_type, "UINT16") == 0) {
        var_attr->dataType = UA_NODEID_NUMERIC(0, UA_NS0ID_UINT16);
        access->data_type = MB_UINT16;

    } else if (strcmp(data_type, "UINT32") == 0) {
        var_attr->dataType = UA_NODEID_NUMERIC(0, UA_NS0ID_UINT32);
        access->data_type = MB_UINT32;

    } else {
        return CONFIG_ERROR;
    }
    return CONFIG_OK;
}

UA_StatusCode Server_setVariableTypeAndInitalValue(cJSON *var_config, Modbus_Access *access, UA_VariableAttributes *var_attr)
{
    char *data_type = cJSON_GetObjectItem(var_config, "DataType")->valuestring;
    if (strcmp(data_type, "FLOAT") == 0) {
        var_attr->dataType = UA_NODEID_NUMERIC(0, UA_NS0ID_FLOAT);

        char *byte_order = cJSON_GetObjectItem(var_config, "ByteOrder")->valuestring;
        if (strcmp(byte_order, "abcd") == 0) access->data_type = MB_FLOAT_ABCD;
        else if (strcmp(byte_order, "badc") == 0) access->data_type = MB_FLOAT_BADC;
        else if (strcmp(byte_order, "cdab") == 0) access->data_type = MB_FLOAT_CDAB;
        else if (strcmp(byte_order, "dcba") == 0) access->data_type = MB_FLOAT_DCBA;
        else return CONFIG_ERROR;
        
        if (cJSON_HasObjectItem(var_config, "InitialValue")) {
            UA_Float init_value = (UA_Float)cJSON_GetObjectItem(var_config, "InitialValue")->valuedouble;
            UA_Variant_setScalar(&var_attr->value, &init_value, &UA_TYPES[UA_TYPES_FLOAT]);
        }

    } else if (strcmp(data_type, "UINT16") == 0) {
        var_attr->dataType = UA_NODEID_NUMERIC(0, UA_NS0ID_UINT16);
        access->data_type = MB_UINT16;

        if (cJSON_HasObjectItem(var_config, "InitialValue")) {
            UA_UInt16 init_value = (UA_UInt16)cJSON_GetObjectItem(var_config, "InitialValue")->valueint;
            UA_Variant_setScalar(&var_attr->value, &init_value, &UA_TYPES[UA_TYPES_UINT16]);
        }

    } else if (strcmp(data_type, "UINT32") == 0) {
        var_attr->dataType = UA_NODEID_NUMERIC(0, UA_NS0ID_UINT32);
        access->data_type = MB_UINT32;

        if (cJSON_HasObjectItem(var_config, "InitialValue")) {
            UA_UInt32 init_value = (UA_UInt32)cJSON_GetObjectItem(var_config, "InitialValue")->valueint;
            UA_Variant_setScalar(&var_attr->value, &init_value, &UA_TYPES[UA_TYPES_UINT32]);
        }

    } else {
        return CONFIG_ERROR;
    }
    return CONFIG_OK;
}