/*
 * @Description: 
 * @Autor: Weihang Shen
 * @Date: 2022-01-26 00:12:29
 * @LastEditors: Weihang Shen
 * @LastEditTime: 2022-02-10 16:15:49
 */

#include <stdlib.h>

#include <cjson/cJSON.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <open62541/plugin/log_stdout.h>
#include "libmodbus/src/modbus.h"

#include "UAServer.h"
#include "utils/HashMap.h"

#define DEFAULT_PORT 4840
#define BUFFER_SIZE 16
#define CONFIG_OK 0
#define CONFIG_ERROR -1
#define CONNECT_ERROR -2

typedef enum
{
    MB_SERIAL, MB_TCP
} MB_CONNECTION_TYPE;

typedef enum
{
    MB_COIL_STATUS, MB_INPUT_STATUS, MB_HOLDING_REGISTER, MB_INPUT_REGISTER
} MB_Variable_Type;

typedef enum
{
    MB_BOOLEAN, MB_FLOAT_ABCD, MB_FLOAT_BADC, MB_FLOAT_CDAB, MB_FLOAT_DCBA, MB_UINT16, MB_UINT32, MB_INT16
} MB_Data_Type;

typedef struct
{
    modbus_t *device;
    MB_Variable_Type var_type;
    MB_Data_Type data_type;
    uint32_t var_address;
    uint32_t bit_offset;

} Modbus_Access;

UA_StatusCode Server_init(char *config);
UA_StatusCode Server_start();
UA_StatusCode Server_stop();
UA_StatusCode Server();
void Server_freeModbus(void *access);

uint32_t Server_getDataByteLength(MB_Data_Type data_type);
void Server_getVariant(MB_Data_Type data_type, uint16_t *src, UA_Variant *dst);
void Server_setRegisters(MB_Data_Type data_type, UA_Variant *src, uint16_t *dst);

void UAServer_beforeRead(UA_Server *server,
			           const UA_NodeId *session_id, void *session_context,
                       const UA_NodeId *node_id, void *node_context,
                       const UA_NumericRange *range, const UA_DataValue *data);
void UAServer_afterWrite(UA_Server *server,
                       const UA_NodeId *session_id, void *session_context,
                       const UA_NodeId *node_id, void *node_context,
                       const UA_NumericRange *range, const UA_DataValue *data);

UA_StatusCode Server_addModbusDevice(cJSON *device_config, MB_CONNECTION_TYPE connection_type);
UA_StatusCode Server_addModbusVariable(cJSON *var_config, UA_NodeId parent_node_id, modbus_t *device);

UA_StatusCode Server_setVariableType(cJSON *var_config, Modbus_Access *access, UA_VariableAttributes *var_attr);
UA_StatusCode Server_setVariableTypeAndInitalValue(cJSON *var_config, Modbus_Access *access,
                                                   UA_VariableAttributes *var_attr, int *has_initial_value);

/*##############################################################################################################################*/

UA_UInt16 port = 0;
UA_Boolean running = false;
cJSON *config_root = NULL;
UA_Server *ua_server = NULL;
HashMap *modbus_access_map = NULL;

UA_StatusCode Server_init(char *config)
{
    config_root = cJSON_Parse(config);
    modbus_access_map = Map_new();

    if (cJSON_HasObjectItem(config_root, "Port") == 0) {
        port = DEFAULT_PORT;
    } else {
        port = cJSON_GetObjectItem(config_root, "Port")->valueint;
    }

    running = false;
    ua_server = UA_Server_new();
    UA_ServerConfig_setMinimal(UA_Server_getConfig(ua_server), port, NULL);
    
    if (cJSON_HasObjectItem(config_root, "ModbusRTUs")) {
        cJSON *modbus_rtus = cJSON_GetObjectItem(config_root, "ModbusRTUs");
        int rtu_count = cJSON_GetArraySize(modbus_rtus);

        if (rtu_count <= 0)
            return CONFIG_ERROR;
        
        for (int i = 0; i < rtu_count; ++i) {
            cJSON *rtu_config = cJSON_GetArrayItem(modbus_rtus, i);
            UA_StatusCode status_code = Server_addModbusDevice(rtu_config, MB_SERIAL);
            if (status_code != 0) return status_code;
        }
    }
    
    if (cJSON_HasObjectItem(config_root, "ModbusTCPs")) {
        cJSON *modbus_tcps = cJSON_GetObjectItem(config_root, "ModbusTCPs");
        int tcp_count = cJSON_GetArraySize(modbus_tcps);

        if (tcp_count <= 0)
            return CONFIG_ERROR;
        
        for (int i = 0; i < tcp_count; ++i) {
            cJSON *rtu_config = cJSON_GetArrayItem(modbus_tcps, i);
            UA_StatusCode status_code = Server_addModbusDevice(rtu_config, MB_TCP);
            if (status_code != 0) return status_code;
        }
    }
	
	return 0;
}

UA_StatusCode Server_start()
{
    running = true;
    UA_StatusCode status_code = UA_Server_run(ua_server, &running);
    return status_code;
}

UA_StatusCode Server_stop()
{
    UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Stopping...");
    running = false;
}

void Server_freeModbus(void *access)
{
    modbus_t *device = ((Modbus_Access *)access)->device;
    modbus_close(device);
    free(access);
}

UA_StatusCode Server_free()
{
    Map_free(modbus_access_map, Server_freeModbus);
    cJSON_free(config_root);
    UA_Server_delete(ua_server);
}

uint32_t Server_getDataByteLength(MB_Data_Type data_type)
{
    switch (data_type)
    {
        case MB_FLOAT_ABCD:
        case MB_FLOAT_BADC:
        case MB_FLOAT_CDAB:
        case MB_FLOAT_DCBA: return 4;
        case MB_UINT16: return 2;
        case MB_UINT32: return 4;
        case MB_INT16: return 2;
        default: return 2;
    }
}

void Server_getVariant(MB_Data_Type data_type, uint16_t *src, UA_Variant *dst)
{
    switch (data_type)
    {
        case MB_FLOAT_ABCD:
            {
                UA_Float new_value = modbus_get_float_abcd(src);
                UA_Variant_setScalarCopy(dst, &new_value, &UA_TYPES[UA_TYPES_FLOAT]);
            }
            break;
        case MB_FLOAT_BADC:
            {
                UA_Float new_value = modbus_get_float_badc(src);
                UA_Variant_setScalarCopy(dst, &new_value, &UA_TYPES[UA_TYPES_FLOAT]);
            }
            break;
        case MB_FLOAT_CDAB:
            {
                UA_Float new_value = modbus_get_float_cdab(src);
                UA_Variant_setScalarCopy(dst, &new_value, &UA_TYPES[UA_TYPES_FLOAT]);
            }
            break;
        case MB_FLOAT_DCBA:
            {
                UA_Float new_value = modbus_get_float_dcba(src);
                UA_Variant_setScalarCopy(dst, &new_value, &UA_TYPES[UA_TYPES_FLOAT]);
            }
            break;
        case MB_UINT16:
            {
                UA_UInt16 new_value = *(uint16_t *)src;
                UA_Variant_setScalarCopy(dst, &new_value, &UA_TYPES[UA_TYPES_UINT16]);
            }
            break;
        case MB_UINT32:
            {
                UA_UInt32 new_value = ((uint32_t)(src[0]) << 16) | (uint32_t)src[1];
                UA_Variant_setScalarCopy(dst, &new_value, &UA_TYPES[UA_TYPES_UINT32]);
            }
            break;

        case MB_INT16:
            {
                UA_Int16 new_value = *(int16_t *)src;
                UA_Variant_setScalarCopy(dst, &new_value, &UA_TYPES[UA_TYPES_INT16]);
            }
            break;
            
        default:
            break;
    }
}

void Server_setRegisters(MB_Data_Type data_type, UA_Variant *src, uint16_t *dst)
{
    switch (data_type)
    {
        case MB_FLOAT_ABCD:
            {
                float new_value = *(UA_Float *)src;
                modbus_set_float_abcd(new_value, dst);
            }
            break;
        case MB_FLOAT_BADC:
            {
                float new_value = *(UA_Float *)src;
                modbus_set_float_badc(new_value, dst);
            }
            break;
        case MB_FLOAT_CDAB:
            {
                float new_value = *(UA_Float *)src;
                modbus_set_float_cdab(new_value, dst);
            }
            break;
        case MB_FLOAT_DCBA:
            {
                float new_value = *(UA_Float *)src;
                modbus_set_float_dcba(new_value, dst);
            }
            break;
        case MB_UINT16:
            {
                uint16_t new_value = *(UA_UInt16 *)src;
                dst[0] = (uint16_t)new_value;
            }
            break;
        case MB_UINT32:
            {
                uint32_t new_value = *(UA_UInt32 *)src;
                dst[0] = (uint16_t)(new_value >> 16);
                dst[1] = (uint16_t)new_value;
            }
            break;

        case MB_INT16:
            {
                int16_t new_value = *(UA_Int16 *)src;
                dst[0] = (uint16_t)new_value;
            }
            break;
            
        default:
            break;
    }
}

void UAServer_beforeRead(UA_Server *server,
			           const UA_NodeId *session_id, void *session_context,
                       const UA_NodeId *node_id, void *node_context,
                       const UA_NumericRange *range, const UA_DataValue *data)
{
    // return;
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Reading data...");
    UA_Variant value;
    // read value from modbus reg
    Modbus_Access *access = (Modbus_Access *)Map_get(modbus_access_map, UA_NodeId_hash(node_id));
    int ret = 0;
    
    switch (access->var_type)
    {
    case MB_COIL_STATUS:
        {
            uint8_t buf[BUFFER_SIZE];
            ret = modbus_read_bits(access->device, access->var_address, access->bit_offset + 1, buf);
            UA_Boolean new_value = buf[access->bit_offset];
            UA_Variant_setScalarCopy(&value, &new_value, &UA_TYPES[UA_TYPES_BOOLEAN]);
        }
        break;
    case MB_INPUT_STATUS:
        {
            uint8_t buf[BUFFER_SIZE];
            ret = modbus_read_input_bits(access->device, access->var_address, access->bit_offset + 1, buf);
            UA_Boolean new_value = buf[access->bit_offset];
            UA_Variant_setScalarCopy(&value, &new_value, &UA_TYPES[UA_TYPES_BOOLEAN]);
        }
        break;
    case MB_HOLDING_REGISTER:
        {
            uint16_t buf[BUFFER_SIZE];
            ret = modbus_read_registers(access->device, access->var_address, Server_getDataByteLength(access->data_type) / 2, buf);
            Server_getVariant(access->data_type, buf, &value);
        }
        break;
    
    case MB_INPUT_REGISTER:
        {
            uint16_t buf[BUFFER_SIZE];
            ret = modbus_read_input_registers(access->device, access->var_address, Server_getDataByteLength(access->data_type) / 2, buf);
            Server_getVariant(access->data_type, buf, &value);
        }
        break;
    default:
        break;
    }

    if (ret == -1) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Fail to read data");
        return;
    }

    UA_Server_writeValueWithoutCallback(server, *node_id, value);
}

void UAServer_afterWrite(UA_Server *server,
                       const UA_NodeId *session_id, void *session_context,
                       const UA_NodeId *node_id, void *node_context,
                       const UA_NumericRange *range, const UA_DataValue *data)
{
    // return;
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Writing data...");
    Modbus_Access *access = (Modbus_Access *)Map_get(modbus_access_map, UA_NodeId_hash(node_id));
    int ret = 0;

    switch (access->var_type)
    {
    case MB_COIL_STATUS:
        {
            uint8_t buf[BUFFER_SIZE];
            int new_value = *(UA_Boolean *)(data->value.data);
            ret = modbus_write_bit(access->device, access->var_address, new_value);
        }
        break;
    case MB_HOLDING_REGISTER:
        {
            uint16_t buf[BUFFER_SIZE];
            Server_setRegisters(access->data_type, data->value.data, buf);
            ret = modbus_write_registers(access->device, access->var_address, Server_getDataByteLength(access->data_type) / 2, buf);
        }
        break;
    default:
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Cannot write INPUT data READ-ONLY");
        break;
    }

    if (ret == -1) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Fail to write data");
        return;
    }
}

UA_StatusCode Server_addModbusDevice(cJSON *device_config, MB_CONNECTION_TYPE connection_type)
{
    int ns_index = cJSON_GetObjectItem(device_config, "NamespaceIndex")->valueint;
    UA_NodeId this_node_id = UA_NODEID_STRING(ns_index, cJSON_GetObjectItem(device_config, "NodeID")->valuestring);
    UA_NodeId ref_node_id = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);

    modbus_t *modbus_device = NULL;

    if (connection_type == MB_SERIAL) {
        modbus_device = modbus_new_rtu(cJSON_GetObjectItem(device_config, "Device")->valuestring,
                                       cJSON_GetObjectItem(device_config, "Baud")->valueint,
                                       cJSON_GetObjectItem(device_config, "Parity")->valuestring[0],
                                       cJSON_GetObjectItem(device_config, "DataBits")->valueint,
                                       cJSON_GetObjectItem(device_config, "StopBits")->valueint);

        if (strcmp(cJSON_GetObjectItem(device_config, "SerialMode")->valuestring, "RS485") == 0) {
            modbus_rtu_set_serial_mode(modbus_device, MODBUS_RTU_RS485);
        } else if (strcmp(cJSON_GetObjectItem(device_config, "SerialMode")->valuestring, "RS232") == 0) {
            modbus_rtu_set_serial_mode(modbus_device, MODBUS_RTU_RS232);
        } else {
            return CONFIG_ERROR;
        }
    } else if (connection_type == MB_TCP) {
        modbus_device = modbus_new_tcp(cJSON_GetObjectItem(device_config, "IP")->valuestring,
                                       cJSON_GetObjectItem(device_config, "Port")->valueint);
    }

    int machine_address = cJSON_GetObjectItem(device_config, "MB_MachineAddress")->valueint;
    modbus_set_slave(modbus_device, machine_address);

    UA_ObjectAttributes obj_attr = UA_ObjectAttributes_default;
    obj_attr.description = UA_LOCALIZEDTEXT("en-US", cJSON_GetObjectItem(device_config, "Description")->valuestring);
    obj_attr.displayName = UA_LOCALIZEDTEXT("en-US", cJSON_GetObjectItem(device_config, "DisplayName")->valuestring);
    UA_Server_addObjectNode(ua_server, this_node_id,
                            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                            ref_node_id,
                            UA_QUALIFIEDNAME(ns_index, cJSON_GetObjectItem(device_config, "QualifiedName")->valuestring),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                            obj_attr, NULL, NULL);

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Trying to connect to Modbus device...");
    if (modbus_connect(modbus_device) != 0) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Connection fail");
        return CONNECT_ERROR;
    }
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Connected");

    if (cJSON_HasObjectItem(device_config, "Variables")) {
        cJSON *variables = cJSON_GetObjectItem(device_config, "Variables");
        int var_count = cJSON_GetArraySize(variables);

        for (int i = 0; i < var_count; ++i) {
            cJSON *var_config = cJSON_GetArrayItem(variables, i);
            UA_StatusCode status_code = Server_addModbusVariable(var_config, this_node_id, modbus_device);
            if (status_code < 0) {
                return status_code;
            }
        }
    }

    return CONFIG_OK;
}

UA_StatusCode Server_addModbusVariable(cJSON *var_config, UA_NodeId parent_node_id, modbus_t *device)
{
    int ns_index = cJSON_GetObjectItem(var_config, "NamespaceIndex")->valueint;
    UA_NodeId this_node_id = UA_NODEID_STRING(ns_index, cJSON_GetObjectItem(var_config, "NodeID")->valuestring);
    UA_NodeId ref_node_id = UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY);

    UA_VariableAttributes var_attr = UA_VariableAttributes_default;
    var_attr.description = UA_LOCALIZEDTEXT("en-US", cJSON_GetObjectItem(var_config, "Description")->valuestring);
    var_attr.displayName = UA_LOCALIZEDTEXT("en-US", cJSON_GetObjectItem(var_config, "DisplayName")->valuestring);
    
    Modbus_Access *access = (Modbus_Access *)malloc(sizeof(Modbus_Access));
    access->device = device;
    access->var_address = cJSON_GetObjectItem(var_config, "VariableAddress")->valueint;
    char *variable_type = cJSON_GetObjectItem(var_config, "VariableType")->valuestring;

    int has_initial_value = false;

    if (strcmp(variable_type, "CoilStatus") == 0) {
        access->var_type = MB_COIL_STATUS;
        access->data_type = MB_BOOLEAN;
        access->bit_offset = cJSON_GetObjectItem(var_config, "BitOffset")->valueint;
        var_attr.dataType = UA_NODEID_NUMERIC(0, UA_NS0ID_BOOLEAN);

        if (cJSON_HasObjectItem(var_config, "InitialValue")) {
            has_initial_value = true;
            UA_Boolean init_value = cJSON_GetObjectItem(var_config, "InitialValue")->valueint;
            UA_Variant_setScalarCopy(&var_attr.value, &init_value, &UA_TYPES[UA_TYPES_BOOLEAN]);
        } else {
            UA_Boolean init_value = 0;
            UA_Variant_setScalarCopy(&var_attr.value, &init_value, &UA_TYPES[UA_TYPES_BOOLEAN]);
        }

        var_attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    } else if (strcmp(variable_type, "InputStatus") == 0) {
        access->var_type = MB_INPUT_STATUS;
        access->data_type = MB_BOOLEAN;
        access->bit_offset = cJSON_GetObjectItem(var_config, "BitOffset")->valueint;
        var_attr.dataType = UA_NODEID_NUMERIC(0, UA_NS0ID_BOOLEAN);

        UA_Boolean init_value = 0;
        UA_Variant_setScalarCopy(&var_attr.value, &init_value, &UA_TYPES[UA_TYPES_BOOLEAN]);

        // Set variable READ_ONLY
        var_attr.accessLevel = UA_ACCESSLEVELMASK_READ;
        
    } else if (strcmp(variable_type, "HoldingRegister") == 0) {
        access->var_type = MB_HOLDING_REGISTER;
        UA_StatusCode status_code = Server_setVariableTypeAndInitalValue(var_config, access, &var_attr, &has_initial_value);
        if (status_code != CONFIG_OK) return status_code;
        var_attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    } else if (strcmp(variable_type, "InputRegister") == 0) {
        access->var_type = MB_INPUT_REGISTER;
        UA_StatusCode status_code = Server_setVariableType(var_config, access, &var_attr);
        if (status_code != CONFIG_OK) return status_code;
        // Set variable READ_ONLY
        var_attr.accessLevel = UA_ACCESSLEVELMASK_READ;

    } else {
        return CONFIG_ERROR;
    }

    UA_Variant init_value;
    UA_Variant_copy(&var_attr.value, &init_value);

    UA_Server_addVariableNode(ua_server,
                              this_node_id,
                              parent_node_id,
                              ref_node_id,
                              UA_QUALIFIEDNAME(ns_index, cJSON_GetObjectItem(var_config, "QualifiedName")->valuestring),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              var_attr, NULL, NULL);
    
    Map_put(modbus_access_map, UA_NodeId_hash(&this_node_id), access);
    // binding read & write callback functions
    UA_ValueCallback callback;
	callback.onRead = UAServer_beforeRead;
	callback.onWrite = UAServer_afterWrite;
	UA_Server_setVariableNode_valueCallback(ua_server, this_node_id, callback);
    
    if (has_initial_value)
        UA_Server_writeValue(ua_server, this_node_id, init_value);
    else
        UA_Server_writeValueWithoutCallback(ua_server, this_node_id, init_value);

    return CONFIG_OK;
}

UA_StatusCode Server_setVariableType(cJSON *var_config, Modbus_Access *access, UA_VariableAttributes *var_attr)
{
    char *data_type = cJSON_GetObjectItem(var_config, "DataType")->valuestring;
    if (strcmp(data_type, "FLOAT") == 0) {
        var_attr->dataType = UA_TYPES[UA_TYPES_FLOAT].typeId;

        UA_Float init_value = 0;
        UA_Variant_setScalarCopy(&var_attr->value, &init_value, &UA_TYPES[UA_TYPES_FLOAT]);

        char *byte_order = cJSON_GetObjectItem(var_config, "ByteOrder")->valuestring;
        if (strcmp(byte_order, "abcd") == 0) access->data_type = MB_FLOAT_ABCD;
        else if (strcmp(byte_order, "badc") == 0) access->data_type = MB_FLOAT_BADC;
        else if (strcmp(byte_order, "cdab") == 0) access->data_type = MB_FLOAT_CDAB;
        else if (strcmp(byte_order, "dcba") == 0) access->data_type = MB_FLOAT_DCBA;
        else return CONFIG_ERROR;

    } else if (strcmp(data_type, "UINT16") == 0) {
        var_attr->dataType = UA_TYPES[UA_TYPES_UINT16].typeId;
        access->data_type = MB_UINT16;

        UA_UInt16 init_value = 0;
        UA_Variant_setScalarCopy(&var_attr->value, &init_value, &UA_TYPES[UA_TYPES_UINT16]);

    } else if (strcmp(data_type, "UINT32") == 0) {
        var_attr->dataType = UA_TYPES[UA_TYPES_UINT32].typeId;
        access->data_type = MB_UINT32;

        UA_UInt32 init_value = 0;
        UA_Variant_setScalarCopy(&var_attr->value, &init_value, &UA_TYPES[UA_TYPES_UINT32]);

    } else if (strcmp(data_type, "INT16") == 0) {
        var_attr->dataType = UA_TYPES[UA_TYPES_INT16].typeId;
        access->data_type = MB_INT16;

        UA_Int16 init_value = 0;
        UA_Variant_setScalarCopy(&var_attr->value, &init_value, &UA_TYPES[UA_TYPES_INT16]);

    } else {
        return CONFIG_ERROR;
    }
    return CONFIG_OK;
}

UA_StatusCode Server_setVariableTypeAndInitalValue(cJSON *var_config, Modbus_Access *access,
                                                   UA_VariableAttributes *var_attr, int *has_initial_value)
{
    char *data_type = cJSON_GetObjectItem(var_config, "DataType")->valuestring;
    if (strcmp(data_type, "FLOAT") == 0) {
        var_attr->dataType = UA_TYPES[UA_TYPES_FLOAT].typeId;

        char *byte_order = cJSON_GetObjectItem(var_config, "ByteOrder")->valuestring;
        if (strcmp(byte_order, "abcd") == 0) access->data_type = MB_FLOAT_ABCD;
        else if (strcmp(byte_order, "badc") == 0) access->data_type = MB_FLOAT_BADC;
        else if (strcmp(byte_order, "cdab") == 0) access->data_type = MB_FLOAT_CDAB;
        else if (strcmp(byte_order, "dcba") == 0) access->data_type = MB_FLOAT_DCBA;
        else return CONFIG_ERROR;
        
        if (cJSON_HasObjectItem(var_config, "InitialValue")) {
            *has_initial_value = true;
            UA_Float init_value = (UA_Float)cJSON_GetObjectItem(var_config, "InitialValue")->valuedouble;
            UA_Variant_setScalarCopy(&var_attr->value, &init_value, &UA_TYPES[UA_TYPES_FLOAT]);
        } else {
            *has_initial_value = false;
            UA_Float init_value = 0;
            UA_Variant_setScalarCopy(&var_attr->value, &init_value, &UA_TYPES[UA_TYPES_FLOAT]);
        }

    } else if (strcmp(data_type, "UINT16") == 0) {
        var_attr->dataType = UA_TYPES[UA_TYPES_UINT16].typeId;
        access->data_type = MB_UINT16;

        if (cJSON_HasObjectItem(var_config, "InitialValue")) {
            *has_initial_value = true;
            UA_UInt16 init_value = (UA_UInt16)cJSON_GetObjectItem(var_config, "InitialValue")->valueint;
            UA_Variant_setScalarCopy(&var_attr->value, &init_value, &UA_TYPES[UA_TYPES_UINT16]);
        } else {
            *has_initial_value = false;
            UA_UInt16 init_value = 0;
            UA_Variant_setScalarCopy(&var_attr->value, &init_value, &UA_TYPES[UA_TYPES_UINT16]);
        }

    } else if (strcmp(data_type, "UINT32") == 0) {
        var_attr->dataType = UA_TYPES[UA_TYPES_UINT32].typeId;
        access->data_type = MB_UINT32;

        if (cJSON_HasObjectItem(var_config, "InitialValue")) {
            *has_initial_value = true;
            UA_UInt32 init_value = (UA_UInt32)cJSON_GetObjectItem(var_config, "InitialValue")->valueint;
            UA_Variant_setScalarCopy(&var_attr->value, &init_value, &UA_TYPES[UA_TYPES_UINT32]);
        } else {
            *has_initial_value = false;
            UA_UInt32 init_value = 0;
            UA_Variant_setScalarCopy(&var_attr->value, &init_value, &UA_TYPES[UA_TYPES_UINT32]);
        }

    } else if (strcmp(data_type, "INT16") == 0) {
        var_attr->dataType = UA_TYPES[UA_TYPES_INT16].typeId;
        access->data_type = MB_INT16;

        if (cJSON_HasObjectItem(var_config, "InitialValue")) {
            *has_initial_value = true;
            UA_Int16 init_value = (UA_Int16)cJSON_GetObjectItem(var_config, "InitialValue")->valueint;
            UA_Variant_setScalarCopy(&var_attr->value, &init_value, &UA_TYPES[UA_TYPES_INT16]);
        } else {
            *has_initial_value = false;
            UA_Int16 init_value = 0;
            UA_Variant_setScalarCopy(&var_attr->value, &init_value, &UA_TYPES[UA_TYPES_INT16]);
        }

    } else {
        *has_initial_value = false;
        return CONFIG_ERROR;
    }
    return CONFIG_OK;
}