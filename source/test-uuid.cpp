#include <ctime>
#include <chrono>
#include <string>
#include <atomic>
#include <sstream>
#include <iostream>
#include <random>
#include <iomanip>

std::string uuid()
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

int main()
{
    for(int i = 0; i < 8; i++)
    {
        std::string str = uuid();
        std::cout << str << std::endl;
    }
}