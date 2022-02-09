# Opcua-Modbus-Server



### 0. Prebuild dependencies

```bash
sudo apt-get install gcc cmake automake autoconf libtool
```



**⚠️ Note: step 1 to 3  should be excuted elsewhere, not in this project**

### 1. Build dependency for open62541

you shall clone my revised open62541 library because I added a new API (UA_Server_writeWithoutCallback) in open62541

```bash
git clone --branch ModbusNewAPI https://github.com/Naplesoul/open62541.git
cd ./open62541
git submodule update --init --recursive
mkdir build && cd build
cmake -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DUA_NAMESPACE_ZERO=FULL ..
make
sudo make install
```

now you can include headers for open62541, for example: `#include <open62541/server.h>`



### 2. Build dependency for cJSON

```bash
git clone --branch v1.7.15 https://github.com/DaveGamble/cJSON.git
cd ./cJSON
mkdir build && cd build
cmake ..
make
sudo make install
```

now you can include headers for cJSON in the project: `#include <cjson/cJSON.h>`



### 3. Build dependency for libmodbus

```bash
git clone --branch v3.1.7 https://github.com/stephane/libmodbus.git
cd ./libmodbus
autoreconf --install --symlink --force
./configure
```

then you should move source files in `./src` and `./config.h` to the project directory

the project directory of this project would be

```bash
.
├── CMakeLists.txt
├── libmodbus
│   ├── config.h
│   └── src
│       ├── modbus.h
│       ...
├── main.c
...
```

then you can include headers for libmodbus in the project: `#include "libmodbus/src/modbus.h"`



### 4. Build opcua_modbus_server

```bash
cmake -B build
cd build
make
```



### 5. Write json configuration 

the example below shows how the config json file is written

```json
{
    "Port": 4840,	// tcp port where the server runs at, if not assigned, default port will be 4840
    "ModbusRTUs":	// json array contains modbus slaves running at serial mode
    [
        {
            "NodeID_NamespaceIndex": 1,
	    "NodeID_ID": "KFR-35GW",
            "DisplayName": "Air Conditioner KFR-35GW",
            "Description": "a smart air conditioner with a temperature sensor and a humidity sensor",
            "QualifiedName": "Air Conditioner",
            "SerialMode": "RS485",		// serial connection mode, "RS485" or "RS232"
	    "DeviceLocation": "/dev/ttyUSB0",	// serial device file on linux
	    "MB_MachineAddress": 1,		// modbus machines address
	    "Baud": 9600,			// serial connection baud rate, 9600, 19200 etc.
	    "Parity": "N",			// verification, "N" for none, "E" for even parity, "O" for odd parity
	    "DataBits": 8,			// data bits defined by modbus device
	    "StopBits": 1,			// data bits defined by modbus device
            "Variables":			// definitions of the variables of the modbus device
            [
                {
                    "NodeID_NamespaceIndex": 1,
                    "NodeID_ID": "Switch-1",
                    "DisplayName": "Air Conditioner Switch-1",
		    "Description": "a coil value controlling the air conditioner to turn on and off",
                    "QualifiedName": "Air Conditioner Switch",
                    "VariableType": "CoilStatus",	// a bit var type which can support read and write
		    "VariableAddress": 0,		// the address of the variable
                    "BitOffset": 1,			// define which bit of the coil register is the value
                    "InitialValue": 0			// define the initial value of the OPCUA data node
                },
                {
                    "NodeID_NamespaceIndex": 1,
                    "NodeID_ID": "WIFI-Connected-1",
                    "DisplayName": "Air Conditioner WIFI-Connected-1",
		    "Description": "a coil value indicating whether the air conditioner is connected to WIFI",
                    "QualifiedName": "Air Conditioner WIFI Connected",
                    "VariableType": "InputStatus",	// a bit var type which is READ-ONLY
		    "VariableAddress": 1,
                    "BitOffset": 2
                },
                {
                    "NodeID_NamespaceIndex": 1,
                    "NodeID_ID": "Temp-1",
                    "DisplayName": "Setted Temperature Temp-1",
		    "Description": "setted temperature of the air conditioner",
                    "QualifiedName": "Setted Temperature",
                    "VariableType": "HoldingRegister",	// a register value type which can support read and write
                    "DataType": "UINT16",		// unsigned int 16, length is 2 Bytes
		    "VariableAddress": 2,
                    "InitialValue": 25
                },
                {
                    "NodeID_NamespaceIndex": 1,
                    "NodeID_ID": "Temp-2",
                    "DisplayName": "Indoor Temperature Temp-2",
		    "Description": "temperature sensor on the air conditioner",
                    "QualifiedName": "Indoor Temperature",
                    "VariableType": "InputRegister",	// a register value type which is READ-ONLY
                    "DataType": "FLOAT",		// float, length is 4 Bytes
                    "ByteOrder": "abcd",		// byte ordering of the float, be "abcd", "badc", "cdab" or "dcba"
		    "VariableAddress": 4
                },
                {
                    "NodeID_NamespaceIndex": 1,
                    "NodeID_ID": "Humid-1",
                    "DisplayName": "Indoor Humidity Humid-1",
		    "Description": "humidity sensor on the air conditioner",
                    "QualifiedName": "Indoor Humidity",
                    "VariableType": "InputRegister",
                    "DataType": "UINT32",		// unsigned int 32, length is 4 Bytes, Big-endian
		    "VariableAddress": 8
                }
            ]
        }
    ],
    "ModbusTCPs":	// json array contains modbus slaves running at TCP mode, NOT SUPPORTED yet
    [
        {

        }
    ]
}
```



### 6. Run

```bash
./build/server example.json
```

