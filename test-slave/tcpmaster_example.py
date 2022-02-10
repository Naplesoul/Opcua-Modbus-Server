from __future__ import print_function

import modbus_tk
import modbus_tk.defines as cst
from modbus_tk import modbus_tcp, hooks
import logging

def main():
    """main"""
    logger = modbus_tk.utils.create_logger("console", level=logging.DEBUG)

    def on_after_recv(data):
        master, bytes_data = data
        logger.info(bytes_data)

    hooks.install_hook('modbus.Master.after_recv', on_after_recv)

    try:

        def on_before_connect(args):
            master = args[0]
            logger.debug("on_before_connect {0} {1}".format(master._host, master._port))

        hooks.install_hook("modbus_tcp.TcpMaster.before_connect", on_before_connect)

        def on_after_recv(args):
            response = args[1]
            logger.debug("on_after_recv {0} bytes received".format(len(response)))

        hooks.install_hook("modbus_tcp.TcpMaster.after_recv", on_after_recv)

        # Connect to the slave
        master = modbus_tcp.TcpMaster(port=4840)
        master.set_timeout(5.0)
        logger.info("connected")

        # execute(slave, function_code, starting_address, quantity_of_x=0, output_value=0, data_format="", expected_length=-1, write_starting_address_FC23=0, number_file=tuple())
        logger.info(master.execute(1, cst.READ_COILS, 0, 1))

        # logger.info(master.execute(1, cst.READ_HOLDING_REGISTERS, 0, 2, data_format='f'))

'''
        # Read and write floats
        master.execute(1, cst.WRITE_MULTIPLE_REGISTERS, starting_address=0, output_value=[3.14], data_format='>f')
        logger.info(master.execute(1, cst.READ_HOLDING_REGISTERS, 0, 2, data_format='>f'))

        # 读保持寄存器
        logger.info(master.execute(1, cst.READ_HOLDING_REGISTERS, 0, 16))
        # 读输入寄存器
        logger.info(master.execute(1, cst.READ_INPUT_REGISTERS, 0, 16))
        # 读线圈寄存器
        logger.info(master.execute(1, cst.READ_COILS, 0, 16))
        # 读离散输入寄存器
        logger.info(master.execute(1, cst.READ_DISCRETE_INPUTS, 0, 16))
        # 单个读写寄存器操作
        # 写寄存器地址为0的保持寄存器
        logger.info(master.execute(1, cst.WRITE_SINGLE_REGISTER, 0, output_value=21))
        logger.info(master.execute(1, cst.READ_HOLDING_REGISTERS, 0, 1))
        # 写寄存器地址为0的线圈寄存器，写入内容为0（位操作）
        logger.info(master.execute(1, cst.WRITE_SINGLE_COIL, 0, output_value=0))
        logger.info(master.execute(1, cst.READ_COILS, 0, 1))
        # 多个寄存器读写操作
        # 写寄存器起始地址为0的保持寄存器，操作寄存器个数为4
        logger.info(master.execute(1, cst.WRITE_MULTIPLE_REGISTERS, 0, output_value=[20,21,22,23]))
        logger.info(master.execute(1, cst.READ_HOLDING_REGISTERS, 0, 4))
        # 写寄存器起始地址为0的线圈寄存器
        logger.info(master.execute(1, cst.WRITE_MULTIPLE_COILS, 0, output_value=[0,0,0,0]))
        logger.info(master.execute(1, cst.READ_COILS, 0, 4))
'''
    except modbus_tk.modbus.ModbusError as exc:
        logger.error("%s- Code=%d", exc, exc.get_exception_code())

if __name__ == "__main__":
    main()
