# Opcua-Modbus-Server



### 0. Prebuild dependencies

```bash
sudo apt-get install gcc cmake automake autoconf libtool
```



**⚠️ Note: step 1 to 3  should be excuted elsewhere, not in this project**

### 1. Build dependency for open62541

```bash
git clone --branch v1.2.3 https://github.com/open62541/open62541.git
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
mkdir build && cd build
cmake ..
make
```



### 5. Run

```bash
./server
```

