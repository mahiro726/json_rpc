/*
    实现一个词典
*/
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/Buffer.h>
#include <iostream>
#include <string>
#include <unordered_map>

class DictServer
{
private:
    void onConnection(const muduo::net::TcpConnectionPtr &conn)
    {
        if (conn -> connected())
        {
            std::cout << "连接建立！\n";
        }
        else
        {
            std::cout << "连接断开！\n";
        }
    }

    void onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp)
    {
        static std::unordered_map<std::string, std::string> dict_map = {
            {"hello", "你好"},
            {"world", "世界"},
            {"bite", "比特"}
        };
        // 从buffer中获取字符串，然后进行翻译的
        std::string msg = buf->retrieveAllAsString();
        std::string res;
        auto it = dict_map.find(msg);
        if (it != dict_map.end())
        {
            res = it->second;
        }
        else
        {
            res = "未知单词！";
        }
        conn->send(res); // 处理过后，就发送数据
    }

public:
    DictServer(int port)
    :_server(&_baseloop, muduo::net::InetAddress("0.0.0.0", port), "DictServer", muduo::net::TcpServer::kReusePort)
    {
        _server.setConnectionCallback(std::bind(&DictServer::onConnection, this, std::placeholders::_1));
        _server.setMessageCallback(std::bind(&DictServer::onMessage,
                                            this,
                                            std::placeholders::_1,
                                            std::placeholders::_2,
                                            std::placeholders::_3));
    }

    void Start()
    {
        // 先开始监听
        _server.start();
        // 再开始死循环事件监控
        _baseloop.loop();
    }

private:
    muduo::net::TcpServer _server;
    muduo::net::EventLoop _baseloop;
};

int main()
{
    DictServer server(8080);
    server.Start();   

    return 0;
}