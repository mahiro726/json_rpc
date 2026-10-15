#include <iostream>
#include <string>
#include <sstream>
#include <memory>
#include <jsoncpp/json/json.h>

// 实现数据的序列化
bool Serialize(const Json::Value &val, std::string &body)
{
    std::stringstream ss;

    // 先实例化一个工厂类对象
    Json::StreamWriterBuilder swb;
    // 通过工厂类对象来生产派生类对象
    std::unique_ptr<Json::StreamWriter> sw (swb.newStreamWriter());
    int n = sw->write(val, &ss); // 成功会返回0
    if (n != 0)
    {
        std::cout << "json serialize failed!\n";
    }
    body = ss.str();

    return true;
}

// 实现Json字符串的反序列化
bool Deserialze(const std::string& body, Json::Value &val)
{
    // 实例化工厂类对象
    Json::CharReaderBuilder crb;
    // 生产CharReader对象
    std::unique_ptr<Json::CharReader> cb(crb.newCharReader());

    std::string errs;
    bool ret = cb->parse(body.c_str(), body.c_str()+body.size(), &val, &errs);
    if (ret == false)
    {
        std::cout << "json deserialize failed! errs: " << errs << std::endl;
        return false;
    }
    return true;
}

int main()
{
    std::string str = R"({"姓名": "小黑", "年龄": 19})";
    Json::Value stu; // 输出型参数
    bool ret = Deserialze(str, stu);
    if (ret == false)
    {
        std::cerr << "反序列化失败";
        return -1;
    }
    std::cout << "姓名：" << stu["姓名"].asString() << std::endl;
    // ……
    return 0;
}