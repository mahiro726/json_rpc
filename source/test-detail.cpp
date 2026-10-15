#include <iostream>
#include <ctime>
#include <cstdio>

#define LDBG 0
#define LINF 1
#define LERR 2

#define LDEFAULT LDBG

// 注释放在宏外，续行符后不追加注释。
#define LOG(level, format, ...) { \
    if ((level) >= LDEFAULT) { \
        std::time_t t = std::time(nullptr); \
        std::tm* lt = std::localtime(&t); \
        char time_tmp[32] = {0}; \
        std::strftime(time_tmp, sizeof(time_tmp), \
                      "%m-%d %H:%M:%S", lt); \
        std::printf("[%s] [%s:%d] " format "\n", \
                    time_tmp, __FILE__, __LINE__, ##__VA_ARGS__); \
    } \
}

#define DLOG(format, ...) LOG(LDBG, format, ##__VA_ARGS__)
#define ILOG(format, ...) LOG(LINF, format, ##__VA_ARGS__)
#define ELOG(format, ...) LOG(LERR, format, ##__VA_ARGS__)

int main()
{
    // // 取得当前时间 → 拆成本地年月日时分秒 → 格式化成字符串
    // //  time            localtime             strftime
    // time_t t = time(nullptr);
    // struct tm* lt = localtime(&t);
    // char time[32] = {0};
    // strftime(time, sizeof(time), "%m-%d %T", lt);

    DLOG("开始运行");
    DLOG("数值=%d", 99);
}