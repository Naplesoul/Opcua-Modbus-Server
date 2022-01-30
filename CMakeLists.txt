cmake_minimum_required(VERSION 3.10)

project(opcua_modbus_server)

add_executable(server UAServer.c utils/HashMap.c main.c)

find_package(open62541 REQUIRED COMPONENTS)

set(CJSON_LIBRARY "-cJSON")
find_package(cJSON REQUIRED)
include_directories(${cJSON_INCLUDE_DIR})

include_directories(
    ./libmodbus
    ./libmodbus/src/
)
add_library(libmodbus SHARED
    ./libmodbus/src/modbus-data.c
    ./libmodbus/src/modbus-tcp.c
    ./libmodbus/src/modbus-rtu.c
    ./libmodbus/src/modbus.c
)

target_link_libraries(server
    ${CJSON_LIBRARIES}
    open62541::open62541
    libmodbus
)