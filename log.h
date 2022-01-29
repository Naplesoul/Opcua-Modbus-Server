#ifndef LOG_H
#define LOG_H

#include <chrono>
#include <sstream>
#include <iomanip>

std::string current_time()
{
    std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
    time_t time = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

#define LOG_INFO(fmt, args...) \
    do { \
        std::string now = current_time();\
        printf("[INFO][%s] " fmt "\n", now.data(), ##args); \
    } while(0);

#define LOG_WARN(fmt, args...) \
    do { \
        std::string now = current_time();\
        printf("[WARN][%s] " fmt "\n", now.data(), ##args); \
    } while(0);

#define LOG_ERRO(fmt, args...) \
    do { \
        std::string now = current_time();\
        printf("[ERRO][%s] " fmt "\n", now.data(), ##args); \
    } while(0);

#endif