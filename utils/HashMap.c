/*
 * @Description: 
 * @Autor: Weihang Shen
 * @Date: 2022-01-30 21:13:11
 * @LastEditors: Weihang Shen
 * @LastEditTime: 2022-02-07 14:03:22
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "HashMap.h"

void List_free(LinkedList *list, void *free_func(void *))
{
    while (list) {
        if (list->next) {
            LinkedList *next = list->next;
            free_func(list->value);
            free(list);
            list = next;
        } else {
            free_func(list->value);
            free(list);
            return;
        }
    }
}

void *List_get(LinkedList *list, uint32_t key)
{
    while (list) {
        if (list->key == key) {
            return list->value;
        }
        list = list->next;
    }
    return NULL;
}

void List_put(LinkedList *list, uint32_t key, void *value)
{
    while (1) {
        if (list->key == key) {
            free(list->value);
            list->value = value;
            return;
        }
        
        if (list->next) {
            list = list->next;
        } else {
            list->next = (LinkedList *)malloc(sizeof(LinkedList));
            list = list->next;
            list->key = key;
            list->value = value;
            list->next = NULL;
            return;
        }
    }
}

HashMap *Map_new()
{
    HashMap *map = (HashMap *)malloc(sizeof(HashMap));
    memset(map, 0, sizeof(HashMap));
}

void Map_free(HashMap *map, void *free_func(void *))
{
    for (uint32_t i = 0; i < MAP_SIZE; ++i) {
        List_free(map->array[i], free_func);
    }
    free(map);
}

void *Map_get(HashMap *map, uint32_t key)
{
    uint32_t index = key % MAP_SIZE;
    return List_get(map->array[index], key);
}

void Map_put(HashMap *map, uint32_t key, void *value)
{
    uint32_t index = key % MAP_SIZE;
    if (map->array[index] == NULL) {
        LinkedList *list = (LinkedList *)malloc(sizeof(LinkedList));
        map->array[index] = list;
        list->key = key;
        list->value = value;
        list->next = NULL;
    } else {
        List_put(map->array[index], key, value);
    }
}