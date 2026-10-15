#include <muduo/net/TcpClient.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/base/CountDownLatch.h>
#include <muduo/net/InetAddress.h>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>

class DictClient
{
public:
    DictClient(const std::string& server_ip , int server_port)
    :_conn(),
     _downlatch(1),
     _loopthread(),
     _baseloop(_loopthread.startLoop()),
     _client(_baseloop, muduo::net::InetAddress(server_ip, server_port), "DictClient")
    {
        _client.setConnectionCallback(std::bind(&DictClient::onConnection, this, std::placeholders::_1));
        _client.setMessageCallback(std::bind(&DictClient::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        
        // 客户端发起异步连接
        _client.connect();        
        // 等待线程 onConnection 连接就绪
        _downlatch.wait();
    }

    bool send(std::string &msg)
    {
        muduo::net::TcpConnectionPtr conn;
        // 加锁获得连接副本
        {
            std::lock_guard<std::mutex> lock(_conn_mutex);
            conn = _conn;
        }

        // 判断有没有拿到连接
        if (!conn)
        {
            // 打印调试信息
            std::cout << "send() 未拿到连接！" << std::endl;
            return false;
        }
        std::cout << "send() 拿到连接！" << std::endl;
        
        // 把发送任务交给后台事件循环
        _baseloop->runInLoop([conn, msg](){
            if(conn->connected())
            {
                conn->send(msg);
            }
        });
        return true;
    }

    ~DictClient()
    {}

private:
    void onConnection(const muduo::net::TcpConnectionPtr& conn)
    {
        // 先判断有没有成功建立连接
        // 如果成果建立连接就保存连接到_conn中
        // 如果连接断开就清空_conn
        // 注意：对_conn操作的过程中，要使用锁来保护
        if (conn->connected())
        {
            // 打印调试信息
            std::cout << "客户端连接建立成功！" << std::endl;
            {
                std::lock_guard<std::mutex> lock(_conn_mutex);
                _conn = conn;
            }
            _downlatch.countDown(); // 再通知：连接已经准备好了
        }
        else
        {
            std::cout << "客户端连接断开！" << std::endl;
            {
                std::lock_guard<std::mutex> lock(_conn_mutex);
                _conn.reset(); // 清空保存的连接
            }
        }
    }

    // 我只需要决定收到数据之后要干什么
    void onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp)
    {
        // 得到接收缓冲区中的数据
        std::string msg = buf->retrieveAllAsString();
        // 得到之后打出来就行了
        std::cout << msg << std::endl;
    }

private:
    muduo::net::TcpConnectionPtr _conn;      // 保存建立好的连接
    std::mutex _conn_mutex;                  // 保护共享的 _conn
    muduo::CountDownLatch _downlatch;        // 让主线程等待连接建立
    muduo::net::EventLoopThread _loopthread; // 管理后台事件循环线程
    muduo::net::EventLoop *_baseloop;        // 指向后台线程中的事件循环
    muduo::net::TcpClient _client;           // 管理客户端连接与回调
};

int main()
{
    DictClient client("127.0.0.1", 8080);

    std::string msg;

    while(true)
    {
        std::cout <<  "请输入单词（quit退出）：" << std::flush;
        if ( !(std::cin>>msg) || msg == "quit")
        {
            std::cout << "客户端退出！" << std::endl;
            break;
        }
        else
        {
            if(!client.send(msg))
            {
                break;
            }
        }
    }
}