#pragma once
#include "net.hpp"
#include "message.hpp"

namespace lccrpc
{
    class Dispatcher
    {
    public:
        using Ptr = std::shared_ptr<Dispatcher>;
        
        void RegisterHandler(MType mtype, const MessageCallback &handler)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            // 注册需要消息类型和对应的消息处理函数
            _handlers.insert(make_pair(mtype, handler));
        }

        void OnMessage(const BaseConnection::Ptr& conn, BaseMessage::Ptr& msg)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            // 找到消息类型对应的函数，调用即可
            MType mtype = msg->GetMType();
            auto it = _handlers.find(mtype);
            
            if (it == _handlers.end())
            {
                ELOG("收到未知类型的消息！, mtype: %d", msg->GetMType());
                conn->Shutdown();
                return;
            }

            it->second(conn, msg);
        }

    private:
        // 识别消息类型，并进行函数匹配
        std::unordered_map<MType, MessageCallback> _handlers;
        // 用锁来进行保护
        std::mutex _mutex;
    };
}