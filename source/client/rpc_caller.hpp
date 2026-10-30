#pragma once
#include "requestor.hpp"
#include <exception>
#include <stdexcept>

namespace lccrpc
{
    namespace client
    {
        class RpcCaller
        {
        public:
            using Ptr = std::shared_ptr<RpcCaller>;
            using JsonAsyncResponse = std::future<Json::Value>;
            using JsonResponseCallback = std::function<void(const Json::Value&)>;

            explicit RpcCaller(const Requestor::Ptr& requestor)
                : _requestor(requestor)
            {}

            // Requestor 处理完整 BaseMessage，RpcCaller 向用户交付其中的 result。
            bool Call(const BaseConnection::Ptr& conn, const std::string& method,
                      const Json::Value& params, Json::Value& result)
            {
                if (!CanCall(conn)) return false;
                auto req_msg = MakeRequest(method, params);
                BaseMessage::Ptr rsp_msg;
                if (!_requestor->Send(conn, req_msg, rsp_msg))
                {
                    ELOG("同步Rpc请求失败！");
                    return false;
                }

                std::string error;
                if (!ExtractResult(rsp_msg, result, error))
                {
                    ELOG("同步Rpc请求出错：%s", error.c_str());
                    return false;
                }
                return true;
            }

            bool Call(const BaseConnection::Ptr& conn, const std::string& method,
                      const Json::Value& params, JsonAsyncResponse& result)
            {
                if (!CanCall(conn)) return false;
                auto req_msg = MakeRequest(method, params);
                auto json_promise = std::make_shared<std::promise<Json::Value>>();
                result = json_promise->get_future();

                // 回调按值持有 promise，Call 返回后仍可设置 JSON 结果。
                Requestor::RequestCallback cb = std::bind(&RpcCaller::Callback,
                    json_promise, std::placeholders::_1);
                if (!_requestor->Send(conn, req_msg, cb))
                {
                    json_promise->set_exception(std::make_exception_ptr(
                        std::runtime_error("异步Rpc请求发送失败")));
                    ELOG("异步Rpc请求失败！");
                    return false;
                }
                return true;
            }

            bool Call(const BaseConnection::Ptr& conn, const std::string& method,
                      const Json::Value& params, const JsonResponseCallback& cb)
            {
                if (!CanCall(conn)) return false;
                if (!cb)
                {
                    ELOG("Rpc结果回调不能为空！");
                    return false;
                }
                auto req_msg = MakeRequest(method, params);
                Requestor::RequestCallback req_cb = std::bind(&RpcCaller::Callback1,
                    cb, std::placeholders::_1);
                if (!_requestor->Send(conn, req_msg, req_cb))
                {
                    ELOG("回调Rpc请求失败！");
                    return false;
                }
                return true;
            }

        private:
            bool CanCall(const BaseConnection::Ptr& conn) const
            {
                if (!_requestor || !conn)
                {
                    ELOG("Rpc调用缺少Requestor或通信连接！");
                    return false;
                }
                return true;
            }

            static RpcRequest::Ptr MakeRequest(const std::string& method, const Json::Value& params)
            {
                auto req_msg = MessageFactory::create<RpcRequest>();
                req_msg->SetId(UUID::uuid());
                req_msg->SetMType(MType::REQ_RPC);
                req_msg->SetMethod(method);
                req_msg->SetParams(params);
                return req_msg;
            }

            static bool ExtractResult(const BaseMessage::Ptr& msg, Json::Value& result,
                                      std::string& error)
            {
                auto rpc_rsp_msg = std::dynamic_pointer_cast<RpcResponse>(msg);
                if (!rpc_rsp_msg)
                {
                    error = "rpc响应向下类型转换失败";
                    return false;
                }
                try
                {
                    if (!rpc_rsp_msg->Check())
                    {
                        error = "rpc响应格式无效";
                        return false;
                    }
                    if (rpc_rsp_msg->GetRCode() != RCode::RCODE_OK)
                    {
                        error = errReason(rpc_rsp_msg->GetRCode());
                        return false;
                    }
                    result = rpc_rsp_msg->GetResult();
                    return true;
                }
                catch (const std::exception& e)
                {
                    error = std::string("rpc响应解析失败：") + e.what();
                    return false;
                }
            }

            // 适配器不访问成员，避免延后响应依赖已销毁的 RpcCaller 的 this。
            static void Callback(const std::shared_ptr<std::promise<Json::Value>>& result,
                                 const BaseMessage::Ptr& msg)
            {
                Json::Value value;
                std::string error;
                if (!ExtractResult(msg, value, error))
                {
                    ELOG("异步Rpc请求出错：%s", error.c_str());
                    result->set_exception(std::make_exception_ptr(std::runtime_error(error)));
                    return;
                }
                result->set_value(value);
            }

            static void Callback1(const JsonResponseCallback& cb, const BaseMessage::Ptr& msg)
            {
                Json::Value value;
                std::string error;
                if (!ExtractResult(msg, value, error))
                {
                    ELOG("回调Rpc请求出错：%s", error.c_str());
                    return;
                }
                // 用户回调异常不能阻断 Requestor 后续删除请求描述。
                try
                {
                    cb(value);
                }
                catch (const std::exception& e)
                {
                    ELOG("Rpc结果回调异常：%s", e.what());
                }
                catch (...)
                {
                    ELOG("Rpc结果回调发生未知异常！");
                }
            }

            Requestor::Ptr _requestor;
        };
    }
}
