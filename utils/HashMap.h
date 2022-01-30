/*
 * @Description: 
 * @Autor: Weihang Shen
 * @Date: 2022-01-30 21:13:02
 * @LastEditors: Weihang Shen
 * @LastEditTime: 2022-01-30 22:10:47
 */
#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdlib.h>

#define MAP_SIZE 128

typedef struct _ListNode
{
    u_int32_t key;
    void *value;
    struct _ListNode *next;
} LinkedList;


typedef struct
{
    LinkedList *array[MAP_SIZE];
} HashMap;

HashMap *Map_new();
void Map_free(HashMap *map);
void *Map_get(HashMap *map, u_int32_t key);
void Map_put(HashMap *map, u_int32_t key, void *value);

#endif