#pragma once

#include "fields.hpp"
#include "detail.hpp"
#include <memory>
#include <functional>

namespace lccrpc
{
    class BaseMessage
    {
    public:
        using Ptr = std::shared_ptr<BaseMessage>;
        virtual ~BaseMessage(){}
        virtual void SetId(const std::string& id)
        {
            _rid = id;
        }
        virtual std::string GetId()
        {
            return _rid;
        }
        virtual void SetMType(MType mtype)
        {
            DLOG("设置消息类型：%d", mtype);
            _mtype = mtype;
        }
        virtual MType GetMType()
        {
            return _mtype;
        }
        virtual std::string Serialize() = 0;
        virtual bool Deserialize(const std::string msg) = 0;
        virtual bool Check() = 0;

    private:
        MType _mtype; // 消息类型
        std::string _rid;
    };

    class BaseBuffer
    {
    public:
        using Ptr = std::shared_ptr<BaseBuffer>;
        virtual size_t ReadableSize() = 0;
        virtual int32_t PeekInt32() = 0;
        virtual int32_t ReadInt32() = 0;
        virtual void RetrieveInt32() = 0;
        virtual std::string RetrieveAsString(size_t len) = 0;
    };

    class BaseProtocol
    {
    public:
        using Ptr = std::shared_ptr<BaseProtocol>; // 将指针放在类中避免命名冲突是更好的做法
        virtual bool OnMessage(BaseBuffer::Ptr& buf, BaseMessage::Ptr& msg) = 0;
        virtual std::string Serialize(const BaseMessage::Ptr& msg) = 0;
        virtual bool CanProcessed(const BaseBuffer::Ptr& buf) = 0;
    };

    class BaseConnection
    {
    public:
        using Ptr = std::shared_ptr<BaseConnection>;
        virtual void Send(const BaseMessage::Ptr& msg) = 0;
        virtual void Shutdown() = 0;
        virtual bool Connected() = 0;
    };

    using ConnectionCallback = std::function<void(BaseConnection::Ptr&)>;
    using CloseCallback = std::function<void(BaseConnection::Ptr&)>;
    using MessageCallback = std::function<void(const BaseConnection::Ptr &, BaseMessage::Ptr&)>;
    class BaseServer
    {
    public:
        using Ptr = std::shared_ptr<BaseServer>;
        virtual void SetConnectionCallback(const ConnectionCallback& cb)
        {
            _cb_connection = cb;
        }
        virtual void SetCloseCallback(const CloseCallback& cb)
        {
            _cb_close = cb;
        }
        virtual void SetMessageCallback(const MessageCallback& cb)
        {
            _cb_message = cb;
        }
        virtual void Start() = 0;
    protected:
        ConnectionCallback _cb_connection;
        CloseCallback _cb_close;
        MessageCallback _cb_message;
    };

    class BaseClient
    {
    public:
        using Ptr = std::shared_ptr<BaseClient>;
        virtual void SetConnectionCallback(const ConnectionCallback& cb)
        {
            _cb_connection = cb;
        }
        virtual void SetCloseCallback(const CloseCallback& cb)
        {
            _cb_close = cb;
        }
        virtual void SetMessageCallback(const MessageCallback& cb)
        {
            _cb_message = cb;
        }
        virtual void Connect() = 0;
        virtual void Shutdown() = 0;
        virtual bool Send(const BaseMessage::Ptr& msg) = 0;
        virtual BaseConnection::Ptr Connection() = 0;
        virtual bool Connected() = 0;
    protected:
        ConnectionCallback _cb_connection;
        CloseCallback _cb_close;
        MessageCallback _cb_message;
    };
};