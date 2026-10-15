#pragma once

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpClient.h>
#include "message.hpp"
#include "abstract.hpp"
#include "fields.hpp"
#include "detail.hpp"
#include <mutex>
#include <unordered_map>

namespace lccrpc
{
    class MuduoBuffer : public BaseBuffer
    {
    public:
        using Ptr = std::shared_ptr<MuduoBuffer>;

        MuduoBuffer(muduo::net::Buffer *buf)
            : _buf(buf)
        {
        }

        virtual size_t ReadableSize() override
        {
            return _buf->readableBytes();
        }

        virtual int32_t PeekInt32() override
        {
            // muduo库是一个网络库，从缓冲区取出一个4字节整形，会进行网络字节序的转换
            return _buf->peekInt32();
        }
        virtual int32_t ReadInt32() override
        {
            return _buf->readInt32();
        }
        virtual void RetrieveInt32() override
        {
            _buf->retrieveInt32();
        }
        virtual std::string RetrieveAsString(size_t len) override
        {
            return _buf->retrieveAsString(len);
        }

    private:
        muduo::net::Buffer *_buf;
    };

    class BufferFactory
    {
    public:
        template <typename... Args>
        static BaseBuffer::Ptr create(Args &&...args)
        {
            return std::make_shared<MuduoBuffer>(std::forward<Args>(args)...);
        }
    };

    class LVProtocol : public BaseProtocol
    {
    public:
        using Ptr = std::shared_ptr<BaseProtocol>;

        // | Len：4字节 |                 Value                 |
        //              | 消息类型 | ID长度 | 请求ID | JSON正文 |

        // 判断缓冲区中的数据量是否足够一条消息的处理
        virtual bool CanProcessed(const BaseBuffer::Ptr &buf) override
        {
            // return buf->ReadableSize() > lenFieldsLength;
            if (buf->ReadableSize() < lenFieldsLength)
            {
                ELOG("缓冲区的数据量不够长度头！");
                return false;
            }
            int32_t value_len = buf->PeekInt32();
            if (buf->ReadableSize() < value_len + lenFieldsLength)
            {
                ELOG("缓冲区的数据量不够一条消息！");
                return false;
            }
            return true;
        }

        // OnMessage 本质上是 parse(解析) + Deserialize(反序列化)
        virtual bool OnMessage(BaseBuffer::Ptr &buf, BaseMessage::Ptr &msg) override
        {
            // 读取value长度
            int32_t value_len = buf->ReadInt32(); // 与peekint32()的区别就是指针是否移动
            // 读取mtype
            MType mtype = (MType)buf->ReadInt32();
            // 读取idlen
            int32_t idlen = buf->ReadInt32();
            // 去掉其他字段，剩下的就是正文长度
            int32_t body_len = value_len - mtyepFieldsLength - idlenFieldsLength - idlen;

            // 取出ID和正文
            std::string id = buf->RetrieveAsString(idlen);
            std::string body = buf->RetrieveAsString(body_len);

            // // 观察一下
            // std::cout << "type = " << static_cast<int>(mtype) << std::endl;
            // std::cout << "id = " << id << std::endl;
            // std::cout << "body = " << body << std::endl;

            msg = MessageFactory::create(mtype);

            if (msg == nullptr)
            {
                ELOG("消息工厂创造对应类型的消息失败！");
                return false;
            }

            if (msg->Deserialize(body) == false)
            {
                ELOG("消息正文反序列化失败！");
                return false;
            }

            msg->SetId(id);
            msg->SetMType(mtype);

            return true;
        }

        virtual std::string Serialize(const BaseMessage::Ptr &msg)
        {
            // 获取body
            std::string body = msg->Serialize();
            // 获取id
            std::string id = msg->GetId();

            // 在主机字节序下计算Value长度
            uint32_t value_len = static_cast<uint32_t>(
                mtyepFieldsLength + idlenFieldsLength + id.size() + body.size());

            // 转换为网络字节序
            uint32_t network_len = htonl(value_len);
            uint32_t network_mtype = htonl(static_cast<uint32_t>(msg->GetMType()));
            uint32_t network_idlen = htonl(static_cast<uint32_t>(id.size()));

            // 预留整包容量，包含最前面的长度头
            std::string result;
            result.reserve(lenFieldsLength + value_len);

            result.append(reinterpret_cast<const char *>(&network_len), lenFieldsLength);
            result.append(reinterpret_cast<const char *>(&network_mtype), mtyepFieldsLength);
            result.append(reinterpret_cast<const char *>(&network_idlen), idlenFieldsLength);
            result.append(id);
            result.append(body);

            return result;
        }

    private:
        const size_t lenFieldsLength = 4;
        const size_t mtyepFieldsLength = 4;
        const size_t idlenFieldsLength = 4;
    };

    class ProtocolFactory
    {
    public:
        template <typename... Args>
        static BaseProtocol::Ptr create(Args &&...args)
        {
            return std::make_shared<LVProtocol>(std::forward<Args>(args)...);
        }
    };

    class MuduoConnection : public BaseConnection
    {
    public:
        using Ptr = std::shared_ptr<MuduoConnection>;

        MuduoConnection(const muduo::net::TcpConnectionPtr &conn, const BaseProtocol::Ptr &protocol)
            : _protocol(protocol),
              _conn(conn)
        {
        }

        virtual void Send(const BaseMessage::Ptr &msg) override
        {
            std::string body = _protocol->Serialize(msg);
            _conn->send(body);
        }

        virtual void Shutdown() override
        {
            _conn->shutdown();
        }

        virtual bool Connected() override
        {
            return _conn->connected();
        }

    private:
        BaseProtocol::Ptr _protocol;        // 没有必要构造对象，类似于一个工具类
        muduo::net::TcpConnectionPtr _conn; // 发送需要连接
    };

    class ConnectionFactory
    {
    public:
        template <typename... Args>
        static BaseConnection::Ptr create(Args &&...args)
        {
            return std::make_shared<MuduoConnection>(std::forward<Args>(args)...);
        }
    };

    class MuduoServer : public BaseServer
    {
    public:
        using Ptr = std::shared_ptr<BaseServer>;

        MuduoServer(int port)
            : _baseloop(),
              _server(&_baseloop, muduo::net::InetAddress("0.0.0.0", port), "MuduoServer", muduo::net::TcpServer::kNoReusePort),
              _protocol(ProtocolFactory::create())
        {
        }

        virtual void Start() override
        {
            _server.setConnectionCallback(std::bind(&MuduoServer::onConnection, this, std::placeholders::_1));
            _server.setMessageCallback(std::bind(&MuduoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _server.start(); // 先将服务器启动(监听)，再开启事件循环
            _baseloop.loop();
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
                auto muduo_conn = ConnectionFactory::create(conn, _protocol);

                {
                    std::lock_guard<std::mutex> lock(_mutex);
                    _conns.insert(std::make_pair(conn, muduo_conn));
                }

                if (_cb_connection)
                {
                    _cb_connection(muduo_conn);
                }
            }
            else
            {
                std::cout << "连接断开！" << std::endl;
                BaseConnection::Ptr muduo_conn;

                {
                    std::lock_guard<std::mutex> lock(_mutex);

                    auto it = _conns.find(conn);
                    if (it == _conns.end())
                    {
                        return; // 没有登记，就没有对应对象需要清理
                    }

                    muduo_conn = it->second; // 先保留一份共享指针
                    _conns.erase(it);        // 再删除管理表中的登记
                } // 离开作用域，自动解锁

                if (_cb_close)
                {
                    _cb_close(muduo_conn); // 锁外通知业务层
                }
            }
        }

        void onMessage(const muduo::net::TcpConnectionPtr &conn,
                       muduo::net::Buffer *buf, muduo::Timestamp)
        {
            auto base_buf = BufferFactory::create(buf);
            BaseConnection::Ptr base_conn;

            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _conns.find(conn);
                if (it == _conns.end())
                {
                    conn->shutdown();
                    return;
                }
                base_conn = it->second;
            }

            while (1)
            {
                if (_protocol->CanProcessed(base_buf) == false)
                {
                    // 数据不足一条完整消息，但积压量已经超过限制
                    if (base_buf->ReadableSize() > maxDataSize)
                    {
                        conn->shutdown();
                        ELOG("缓冲区中数据过大！");
                        return;
                    }

                    break; // 保留剩余数据，等待下一次接收
                }

                BaseMessage::Ptr msg;
                bool ret = _protocol->OnMessage(base_buf, msg);
                if (ret == false)
                {
                    conn->shutdown();
                    ELOG("缓冲区中数据错误！");
                    return;
                }

                // 每解析成功一条消息，就交给业务回调
                if (_cb_message)
                {
                    _cb_message(base_conn, msg);
                }
            }
        }

    private:
        const size_t maxDataSize = (1 << 16);
        BaseProtocol::Ptr _protocol;
        muduo::net::EventLoop _baseloop; // 负责事件的循环
        muduo::net::TcpServer _server;   // 负责服务器的监听、连接管理和回调配置
        std::mutex _mutex;
        std::unordered_map<muduo::net::TcpConnectionPtr, BaseConnection::Ptr> _conns;
    };

    class ServerFactory
    {
    public:
        template <typename... Args>
        static BaseServer::Ptr create(Args &&...args)
        {
            return std::make_shared<MuduoServer>(std::forward<Args>(args)...);
        }
    };

    class MuduoClient : public BaseClient
    {
    public:
        using Ptr = std::shared_ptr<BaseClient>;

        MuduoClient(const std::string &server_ip, int server_port)
            : _protocol(ProtocolFactory::create()),
              _conn(),
              _downlatch(1),
              _loopthread(),
              _baseloop(_loopthread.startLoop()),
              _client(_baseloop, muduo::net::InetAddress(server_ip, server_port), "MuduoClient")
        {
        }

        virtual void Connect() override
        {
            _client.setConnectionCallback(std::bind(&MuduoClient::onConnection, this, std::placeholders::_1));
            _client.setMessageCallback(std::bind(&MuduoClient::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

            // 客户端发起异步连接
            _client.connect();
            // 等待线程 onConnection 连接就绪
            _downlatch.wait();
        }

        virtual void Shutdown() override
        {
            _client.disconnect();
        }

        virtual bool Send(const BaseMessage::Ptr &msg) override
        {
            if (Connected() == false)
            {
                ELOG("连接已断开！");
                return false;
            }
            _conn->Send(msg);
            return true;
        }

        virtual BaseConnection::Ptr Connection() override
        {
            return _conn;
        }

        virtual bool Connected() override
        {
            return (_conn && _conn->Connected());
        }

    private:
        void onConnection(const muduo::net::TcpConnectionPtr &conn)
        {
            if (conn->connected())
            {
                // 打印调试信息
                std::cout << "客户端连接建立成功！" << std::endl;
                _downlatch.countDown(); // 计数--，为0时唤醒阻塞
                _conn = ConnectionFactory::create(conn, _protocol);
            }
            else
            {
                std::cout << "客户端连接断开！" << std::endl;
                _conn.reset(); // 清空保存的连接
            }
        }

        void onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf, muduo::Timestamp)
        {
            auto base_buf = BufferFactory::create(buf);

            while (1)
            {
                if (_protocol->CanProcessed(base_buf) == false)
                {
                    // 数据不足一条完整消息，但积压量已经超过限制
                    if (base_buf->ReadableSize() > maxDataSize)
                    {
                        conn->shutdown();
                        ELOG("缓冲区中数据过大！");
                        return;
                    }

                    break; // 保留剩余数据，等待下一次接收
                }

                BaseMessage::Ptr msg;
                bool ret = _protocol->OnMessage(base_buf, msg);
                if (ret == false)
                {
                    conn->shutdown();
                    ELOG("缓冲区中数据错误！");
                    return;
                }

                // 每解析成功一条消息，就交给业务回调
                if (_cb_message)
                {
                    _cb_message(_conn, msg);
                }
            }
        }

    private:
        const size_t maxDataSize = (1 << 16);
        BaseProtocol::Ptr _protocol;
        BaseConnection::Ptr _conn;               // 保存建立好的连接
        muduo::CountDownLatch _downlatch;        // 让主线程等待连接建立
        muduo::net::EventLoopThread _loopthread; // 管理后台事件循环线程
        muduo::net::EventLoop *_baseloop;        // 指向后台线程中的事件循环
        muduo::net::TcpClient _client;           // 管理客户端连接与回调
    };

    class ClientFactory
    {
    public:
        template <typename... Args>
        static BaseClient::Ptr create(Args &&...args)
        {
            return std::make_shared<MuduoClient>(std::forward<Args>(args)...);
        }
    };
}