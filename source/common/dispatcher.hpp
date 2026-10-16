#pragma once
#include "net.hpp"
#include "message.hpp"

namespace lccrpc
{
    // 非模板基类：让不同类型的回调包装对象可以放进同一张映射表
    class Callback
    {
    public:
        using Ptr = std::shared_ptr<Callback>;
        virtual ~Callback() = default;
        virtual void OnMessage(const BaseConnection::Ptr& conn, BaseMessage::Ptr& msg) = 0;
    };

    // T 是消息类型，例如 RpcRequest、TopicRequest，不是函数类型
    template<typename T>
    class CallbackT : public Callback
    {
    public:
        using Ptr = std::shared_ptr<CallbackT<T>>;
        using MessageCallback = std::function<void(const BaseConnection::Ptr&, std::shared_ptr<T>&)>;

        explicit CallbackT(const MessageCallback& handler)
            : _handler(handler)
        {}

        void OnMessage(const BaseConnection::Ptr& conn, BaseMessage::Ptr& msg) override
        {
            auto type_msg = std::dynamic_pointer_cast<T>(msg);
            // 补充保护：消息实际类型必须与注册时指定的 T 一致
            if (!type_msg)
            {
                ELOG("消息实际类型与注册的回调类型不匹配！");
                conn->Shutdown();
                return;
            }
            _handler(conn, type_msg);
        }

    private:
        MessageCallback _handler;
    };

    class Dispatcher
    {
    public:
        using Ptr = std::shared_ptr<Dispatcher>;

        template<typename T>
        void RegisterHandler(MType mtype, const typename CallbackT<T>::MessageCallback& handler)
        {
            auto cb = std::make_shared<CallbackT<T>>(handler);
            std::lock_guard<std::mutex> lock(_mutex);
            // 保存包装对象，由基类指针统一管理不同类型的业务回调
            _handlers.insert(std::make_pair(mtype, cb));
        }

        // 主干不变：OnMessage 根据消息类型选择业务处理，映射表为此服务
        void OnMessage(const BaseConnection::Ptr& conn, BaseMessage::Ptr& msg)
        {
            MType mtype = msg->GetMType();
            Callback::Ptr cb;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                auto it = _handlers.find(mtype);
                if (it != _handlers.end())
                {
                    cb = it->second;
                }
            }

            if (!cb)
            {
                ELOG("收到未知类型的消息！mtype: %d", static_cast<int>(mtype));
                conn->Shutdown();
                return;
            }

            // 补充改进：查表后释放锁，再执行业务回调
            cb->OnMessage(conn, msg);
        }

    private:
        std::unordered_map<MType, Callback::Ptr> _handlers;
        std::mutex _mutex;
    };
}
