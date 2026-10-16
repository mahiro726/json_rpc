/*
    琐碎功能代码：日志宏、序列化和反序列化、uuid的生成
*/
#pragma once

#include <iostream>
#include <ctime>
#include <cstdio>
#include <string>
#include <sstream>
#include <memory>
#include <jsoncpp/json/json.h>
#include <ctime>
#include <chrono>
#include <atomic>
#include <sstream>
#include <random>
#include <iomanip>

namespace lccrpc
{
#define LDBG 0
#define LINF 1
#define LERR 2

#define LDEFAULT LDBG

// 注释放在宏外，续行符后不追加注释。
#define LOG(level, format, ...)                                       \
    {                                                                 \
        if ((level) >= LDEFAULT)                                      \
        {                                                             \
            std::time_t t = std::time(nullptr);                       \
            std::tm *lt = std::localtime(&t);                         \
            char time_tmp[32] = {0};                                  \
            std::strftime(time_tmp, sizeof(time_tmp),                 \
                          "%m-%d %H:%M:%S", lt);                      \
            std::printf("[%s] [%s:%d] " format "\n",                  \
                        time_tmp, __FILE__, __LINE__, ##__VA_ARGS__); \
        }                                                             \
    }

#define DLOG(format, ...) LOG(LDBG, format, ##__VA_ARGS__);
#define ILOG(format, ...) LOG(LINF, format, ##__VA_ARGS__);
#define ELOG(format, ...) LOG(LERR, format, ##__VA_ARGS__);

    class JsonUtil
    {
    public:
        // 实现数据的序列化
        static bool Serialize(const Json::Value &val, std::string &body)
        {
            std::stringstream ss;

            // 先实例化一个工厂类对象
            Json::StreamWriterBuilder swb;
            // 通过工厂类对象来生产派生类对象
            std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());
            int n = sw->write(val, &ss); // 成功会返回0
            if (n != 0)
            {
                // std::cout << "json serialize failed!\n";
                ELOG("json serialize failed!");
            }
            body = ss.str();

            return true;
        }

        // 实现Json字符串的反序列化
        static bool Deserialze(const std::string &body, Json::Value &val)
        {
            // 实例化工厂类对象
            Json::CharReaderBuilder crb;
            // 生产CharReader对象
            std::unique_ptr<Json::CharReader> cb(crb.newCharReader());

            std::string errs;
            bool ret = cb->parse(body.c_str(), body.c_str() + body.size(), &val, &errs);
            if (ret == false)
            {
                // std::cout << "json deserialize failed! errs: " << errs << std::endl;
                ELOG("json deserialize failed! errs: %s", errs.c_str());
                return false;
            }
            return true;
        }
    };

    class UUID
    {
        static std::string uuid()
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<int> dist(0, 255);

            std::stringstream ss;

            // 设置这个流的输出方式
            ss << std::hex << std::setfill('0');

            for (int i = 0; i < 8; ++i)
            {
                if (i == 4 || i == 6)
                {
                    ss << '-';
                }

                int num = dist(gen);
                ss << std::setw(2) << num;
            }

            // 随机部分与序号部分之间
            ss << '-';

            static std::atomic<std::uint64_t> seq{1};
            std::uint64_t current = seq.fetch_add(1);

            for (int i = 7; i >= 0; --i)
            {
                if (i == 5)
                {
                    ss << '-';
                }

                std::uint64_t byte = (current >> (i * 8)) & 0xff;
                ss << std::setw(2) << byte;
            }

            // 此时才取出完整字符串
            std::string id = ss.str();
            return id;
        }
    };
}