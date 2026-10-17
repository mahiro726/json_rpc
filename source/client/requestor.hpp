#pragma once
#include "../common/abstract.hpp"
#include "../common/message.hpp"
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace lccrpc
{
    namespace client
    {
        class Requestor
        {
        public:
            using Ptr = std::shared_ptr<Requestor>;
            using RequestCallback = std::function<void (const BaseMessage::Ptr&)>;
            using AsyncResponse = std::future<BaseMessage::Ptr>;

            // 接收入口，之后注册给客户端 Dispather
            void OnResponse(const BaseConnection::Ptr& conn, BaseMessage::Ptr& msg);

            // 异步方式：通过输出参数交给上层一个future
            bool Send(const BaseConnection::Ptr& conn, const BaseMessage::Ptr& req, AsyncResponse& async_rsp);

            // 回调方式：记录收到本次响应后该调用的函数
            bool Send(const BaseConnection::Ptr& conn, const BaseMessage::Ptr& req, const RequestCallback& cb);


            // 内部类 - 请求描述
            struct RequestDescribe
            {
                using Ptr = std::shared_ptr<RequestDescribe>;
            public:
                // 请求消息
                // 请求消息类型
                // promise响应
                // 处理响应的回调函数
                BaseMessage::Ptr _request;
                RType _rtype;
                std::promise<BaseMessage::Ptr> _response;
                RequestCallback _callback;
            };

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, RequestDescribe::Ptr> _requests;
        };
    }
}