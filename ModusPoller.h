/*
 * @Description: 
 * @Autor: Weihang Shen
 * @Date: 2022-01-29 00:05:27
 * @LastEditors: Weihang Shen
 * @LastEditTime: 2022-01-29 00:10:30
 */

#include <stdlib.h>

typedef struct
{
    uint16_t port;
} ModusPoller;

uint32_t read_reg(ModusPoller *poller, uint64_t address, double *ret);