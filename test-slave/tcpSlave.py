'''
Description: 
Autor: Weihang Shen
Date: 2022-02-10 13:02:35
LastEditors: Weihang Shen
LastEditTime: 2022-02-10 13:06:21
'''
import sys

import modbus_tk
import modbus_tk.defines as cst
from modbus_tk import modbus_tcp

# from modbus_tk import modbus_rtu
# import serial

logger = modbus_tk.utils.create_logger(name="console", record_format="%(message)s")
server = modbus_tcp.TcpServer(port=5502, address='127.0.0.1')
# server = modbus_rtu.RtuServer(serial.Serial("/dev/ttyAMA0", 9600))

def setup():

    slave_1 = server.add_slave(1)
    
    # Switch-1
    slave_1.add_block("switch", cst.COILS, 0, 1)
    slave_1.set_values("switch", 0, [0])

    # WIFI-Connected-1
    slave_1.add_block("wifi", cst.DISCRETE_INPUTS, 10000, 1)
    slave_1.set_values("wifi", 10000, [0])

    # Temp-1
    # dataType: uint16
    slave_1.add_block("temp1", cst.HOLDING_REGISTERS, 40000, 2)
    slave_1.set_values("temp1", 40000, [25])

    # Temp-2
    # dataType: float
    slave_1.add_block("temp2", cst.ANALOG_INPUTS, 20000, 4)
        
    # Humid-1
    # dataType: uint32
    slave_1.add_block("humid", cst.ANALOG_INPUTS, 20004, 4)
    slave_1.set_values("humid", 20004, [000, 75])


if __name__ == "__main__":
    setup()
    try:
        server.start()

        while True:
            cmd = sys.stdin.readline()
            args = cmd.split(' ')

            if cmd.find('quit') == 0:
                sys.stdout.write('bye-bye\r\n')
                break

            elif args[0] == 'add_slave':
                slave_id = int(args[1])
                server.add_slave(slave_id)
                sys.stdout.write('done: slave %d added\r\n' % slave_id)

            elif args[0] == 'add_block':
                slave_id = int(args[1])
                name = args[2]
                block_type = int(args[3])
                starting_address = int(args[4])
                length = int(args[5])
                slave = server.get_slave(slave_id)
                slave.add_block(name, block_type, starting_address, length)
                sys.stdout.write('done: block %s added\r\n' % name)

            elif args[0] == 'set_values':
                slave_id = int(args[1])
                name = args[2]
                address = int(args[3])
                values = []
                for val in args[4:]:
                    values.append(int(val))
                slave = server.get_slave(slave_id)
                slave.set_values(name, address, values)
                values = slave.get_values(name, address, len(values))
                sys.stdout.write('done: values written: %s\r\n' % str(values))

            elif args[0] == 'get_values':
                slave_id = int(args[1])
                name = args[2]
                address = int(args[3])
                length = int(args[4])
                slave = server.get_slave(slave_id)
                values = slave.get_values(name, address, length)
                sys.stdout.write('done: values read: %s\r\n' % str(values))

            else:
                sys.stdout.write("unknown command %s\r\n" % args[0])
    finally:
        server.stop()