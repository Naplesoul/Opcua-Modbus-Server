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

#define LOG_ERRO(fmt, args...) \
    do { \
        printf("[ERRO]: " fmt "\n", ##args); \
    } while(0);

#endif