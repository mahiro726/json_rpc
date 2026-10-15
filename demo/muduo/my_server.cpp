#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>

class DictServer
{
public:
    DictServer(int port)
        : _baseloop(),
          _server(&_baseloop, muduo::net::InetAddress("0.0.0.0", port), "DictServer", muduo::net::TcpServer::kNoReusePort)
    {
        _server.setConnectionCallback(std::bind(&DictServer::onConnection, this, std::placeholders::_1));
        _server.setMessageCallback(std::bind(&DictServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }

    void Start()
    {
        _server.start(); // 先将服务器启动，再开启事件循环
        _baseloop.loop();
    }

    ~DictServer()
    {
    }

private:
    // Muduo 发现连接建立或断开后，调用这个函数。
    // conn 是 Muduo 交给你的连接对象。
    void onConnection(const muduo::net::TcpConnectionPtr &conn)
    {
        // 仅仅查询是否处于连接状态
        if (conn->connected())
        {
            std::cout << "已经建立连接！" << std::endl;
        }
        else
        {
            std::cout << "连接断开！" << std::endl;
        }
    }

    // onMessage 的需求是：客户端发来一个单词，服务端查出翻译，再通过原来的连接发回去
    void onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf, muduo::Timestamp)
    {
        // 1. 准备词典：单词是 key，翻译是 value
        // static：首次执行到这里时初始化，后续调用复用同一张表
        static std::unordered_map<std::string, std::string> dict_map = {
            {"hello", "你好"},
            {"world", "世界"},
            {"bite", "比特"}
        };

        // 2. 取出缓冲区当前全部可读内容，作为待翻译的单词
        // 同时把这些内容标记为已消费
        std::string msg = buf->retrieveAllAsString();

        // 3. 准备存放本次响应
        std::string resp;

        // 4. 用单词作为 key 查词典
        auto it = dict_map.find(msg);

        // 5. 判断是否找到，找到后才能访问 it 指向的元素
        if (it == dict_map.end())
        {
            resp = "未找到单词！";
        }
        else 
        {
            resp = it->second;
        }

        // 6. 通过发来请求的那条连接发送响应
        conn->send(resp);
    }

private:
    muduo::net::EventLoop _baseloop; // 负责事件的循环
    muduo::net::TcpServer _server;   // 负责服务器的监听、连接管理和回调配置
};

int main()
{
    DictServer server(8080);
    server.Start();
}