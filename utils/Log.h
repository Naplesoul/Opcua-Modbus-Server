/*
  * @Description: 
  * @Autor: Weihang Shen
  * @Date: 2022-01-30 00:19:46
  * @LastEditors: Weihang Shen
  * @LastEditTime: 2022-01-30 16:11:23
  */

 #ifndef LOG_H
 #define LOG_H

 #define LOG_INFO(fmt, args...) \
     do { \
         printf("[INFO]: " fmt "\n", ##args); \
     } while(0);

 #define LOG_WARN(fmt, args...) \
     do { \
         printf("[WARN]: " fmt "\n", ##args); \
     } while(0);

 #define LOG_ERROR(fmt, args...) \
     do { \
         printf("[ERRO]: " fmt "\n", ##args); \
     } while(0);

 #endif 