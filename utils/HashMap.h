/*
 * @Description: 
 * @Autor: Weihang Shen
 * @Date: 2022-01-30 21:13:02
 * @LastEditors: Weihang Shen
 * @LastEditTime: 2022-02-03 20:58:28
 */
#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdint.h>

#define MAP_SIZE 128

typedef struct _ListNode
{
    uint32_t key;
    void *value;
    struct _ListNode *next;
} LinkedList;


typedef struct
{
    LinkedList *array[MAP_SIZE];
} HashMap;

HashMap *Map_new();
void Map_free(HashMap *map);
void *Map_get(HashMap *map, uint32_t key);
void Map_put(HashMap *map, uint32_t key, void *value);

#endif