#pragma once
#include "../common/abstract.hpp"
#include "../common/message.hpp"
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

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

            // 内部类 - 请求描述
            struct RequestDescribe
            {
                using Ptr = std::shared_ptr<RequestDescribe>;

                // 请求消息
                BaseMessage::Ptr _request;
                // 请求的响应交付类型
                RType _rtype;
                // promise响应
                std::promise<BaseMessage::Ptr> _response;
                // 处理响应的回调函数
                RequestCallback _callback;
            };

            // 接收入口，之后注册给客户端 Dispatcher
            void OnResponse(const BaseConnection::Ptr& conn, BaseMessage::Ptr& msg)
            {
                (void)conn;
                const std::string rid = msg->GetId();
                auto rd = GetDescribe(rid);
                if (!rd)
                {
                    ELOG("收到响应 - %s，但是未找到对应的请求描述！", rid.c_str());
                    return;
                }

                if (rd->_rtype == RType::REQ_ASYNC)
                {
                    rd->_response.set_value(msg);
                }
                else if (rd->_rtype == RType::REQ_CALLBACK)
                {
                    if (rd->_callback)
                    {
                        rd->_callback(msg);
                    }
                }
                else
                {
                    ELOG("请求类型未知！");
                }
                DelDescribe(rid);
            }

            // 异步方式：通过输出参数交给上层一个future
            bool Send(const BaseConnection::Ptr& conn, const BaseMessage::Ptr& req, AsyncResponse& async_rsp)
            {
                auto rd = NewDescribe(req, RType::REQ_ASYNC);
                if (!rd)
                {
                    ELOG("构造请求描述对象失败！");
                    return false;
                }
                conn->Send(req);
                async_rsp = rd->_response.get_future();
                return true;
            }

            // 同步方式：复用异步发送，等待完整响应返回
            bool Send(const BaseConnection::Ptr& conn, const BaseMessage::Ptr& req, BaseMessage::Ptr& rsp)
            {
                AsyncResponse rsp_future;
                if (!Send(conn, req, rsp_future))
                {
                    return false;
                }
                rsp = rsp_future.get();
                return true;
            }

            // 回调方式：记录收到本次响应后该调用的函数
            bool Send(const BaseConnection::Ptr& conn, const BaseMessage::Ptr& req, const RequestCallback& cb)
            {
                auto rd = NewDescribe(req, RType::REQ_CALLBACK, cb);
                if (!rd)
                {
                    ELOG("构造请求描述对象失败！");
                    return false;
                }
                conn->Send(req);
                return true;
            }

        private:
            RequestDescribe::Ptr NewDescribe(const BaseMessage::Ptr& req, RType rtype,
                                            const RequestCallback& cb = RequestCallback())
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto rd = std::make_shared<RequestDescribe>();
                rd->_request = req;
                rd->_rtype = rtype;
                if (rtype == RType::REQ_CALLBACK && cb)
                {
                    rd->_callback = cb;
                }
                _requests.insert(std::make_pair(req->GetId(), rd));
                return rd;
            }

            RequestDescribe::Ptr GetDescribe(const std::string& rid)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _requests.find(rid);
                if (it == _requests.end())
                {
                    return RequestDescribe::Ptr();
                }
                return it->second;
            }

            void DelDescribe(const std::string& rid)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _requests.erase(rid);
            }

            std::mutex _mutex;
            std::unordered_map<std::string, RequestDescribe::Ptr> _requests;
        };
    }
}
