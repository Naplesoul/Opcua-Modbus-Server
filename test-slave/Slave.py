'''
Description: 
Autor: Le Chen
Date: 2022-02-08 21:53:29
LastEditors: Weihang Shen
LastEditTime: 2022-02-09 21:24:05
'''
import sys

import modbus_tk
import modbus_tk.defines as cst
from modbus_tk import modbus_tcp, hooks

# from modbus_tk import modbus_rtu
# import serial

logger = modbus_tk.utils.create_logger(name="console", record_format="%(message)s")
server = modbus_tcp.TcpServer(port=5502, address='127.0.0.1')
# server = modbus_rtu.RtuServer(serial.Serial("/dev/ttyAMA0", 9600))

def setup():

    slave_1 = server.add_slave(1)
    
    # Switch-1
    slave_1.add_block("switch", cst.COILS, 0, 1)
    slave_1.set_values("switch", 0, [1])

    # WIFI-Connected-1
    slave_1.add_block("wifi", cst.DISCRETE_INPUTS, 10000, 1)
    # slave_1.set_values("wifi", 10000, [1])

    # Temp-1
    # dataType: uint16
    slave_1.add_block("temp1", cst.HOLDING_REGISTERS, 40000, 2)
    slave_1.set_values("temp1", 40000, [250])

    # Temp-2
    # dataType: float
    slave_1.add_block("temp2", cst.ANALOG_INPUTS, 20000, 4)
    # slave_1.set_values("temp2", 20000, [89.9])
        
    # Humid-1
    # dataType: uint32
    slave_1.add_block("humid", cst.ANALOG_INPUTS, 20004, 4)
    slave_1.set_values("humid", 20004, [000, 567])


if __name__ == "__main__":
    setup()
    try:
        server.start()

    except KeyboardInterrupt:
        server.stop()