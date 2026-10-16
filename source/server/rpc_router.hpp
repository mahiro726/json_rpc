#pragma once

#include "../common/message.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lccrpc
{
    namespace server
    {
        enum class VType
        {
            BOOL = 0,
            INTEGRAL,
            NUMERIC,
            STRING,
            ARRAY,
            OBJECT
        };

        // 保存一个方法的配置；配置通过构造函数一次性传入。
        class ServiceDescribe
        {
        public:
            using Ptr = std::shared_ptr<ServiceDescribe>;
            using ServiceCallback =
                std::function<void(const Json::Value&, Json::Value&)>;
            using ParamsDescribe = std::pair<std::string, VType>;

            ServiceDescribe(std::string&& name,
                            std::vector<ParamsDescribe>&& params_desc,
                            VType return_type, ServiceCallback&& callback)
                : _method_name(std::move(name)),
                  _callback(std::move(callback)),
                  _params_desc(std::move(params_desc)),
                  _return_type(return_type)
            {}

            const std::string& method() const { return _method_name; }

            bool paramCheck(const Json::Value& params) const
            {
                if (!params.isObject())
                {
                    ELOG("RPC参数必须是JSON对象！");
                    return false;
                }
                for (const auto& desc : _params_desc)
                {
                    if (!params.isMember(desc.first))
                    {
                        ELOG("参数字段完整性校验失败！%s 字段缺失！", desc.first.c_str());
                        return false;
                    }
                    if (!check(desc.second, params[desc.first]))
                    {
                        ELOG("%s 参数类型校验失败！", desc.first.c_str());
                        return false;
                    }
                }
                return true;
            }

            bool call(const Json::Value& params, Json::Value& result) const
            {
                if (!_callback)
                {
                    ELOG("服务没有设置业务回调！");
                    return false;
                }
                _callback(params, result);
                if (!rtypeCheck(result))
                {
                    ELOG("回调处理函数的结果类型校验失败！");
                    return false;
                }
                return true;
            }

        private:
            bool rtypeCheck(const Json::Value& value) const
            {
                return check(_return_type, value);
            }

            static bool check(VType type, const Json::Value& value)
            {
                switch (type)
                {
                case VType::BOOL:     return value.isBool();
                case VType::INTEGRAL: return value.isIntegral();
                case VType::NUMERIC:  return value.isNumeric();
                case VType::STRING:   return value.isString();
                case VType::ARRAY:    return value.isArray();
                case VType::OBJECT:   return value.isObject();
                }
                return false;
            }

            std::string _method_name;
            ServiceCallback _callback;
            std::vector<ParamsDescribe> _params_desc;
            VType _return_type;
        };

        // 先逐项收集配置，再 build；每次构建后需重新配置再复用。
        class SDescribeFactory
        {
        public:
            void setMethodName(const std::string& name) { _method_name = name; }
            void setReturnType(VType type) { _return_type = type; }
            void setParamsDesc(const std::string& name, VType type)
            {
                _params_desc.emplace_back(name, type);
            }
            void setCallback(const ServiceDescribe::ServiceCallback& callback)
            {
                _callback = callback;
            }

            ServiceDescribe::Ptr build()
            {
                if (_method_name.empty() || !_callback)
                {
                    throw std::invalid_argument("服务描述需要方法名称和业务回调");
                }
                return std::make_shared<ServiceDescribe>(
                    std::move(_method_name), std::move(_params_desc),
                    _return_type, std::move(_callback));
            }

        private:
            std::string _method_name;
            ServiceDescribe::ServiceCallback _callback;
            std::vector<ServiceDescribe::ParamsDescribe> _params_desc;
            VType _return_type = VType::BOOL;
        };

        class ServiceManager
        {
        public:
            using Ptr = std::shared_ptr<ServiceManager>;

            void insert(const ServiceDescribe::Ptr& desc)
            {
                if (!desc)
                {
                    throw std::invalid_argument("不能注册空服务描述");
                }
                std::lock_guard<std::mutex> lock(_mutex);
                // 与课程一致：同名方法保留首次注册的描述。
                _services.insert(std::make_pair(desc->method(), desc));
            }

            ServiceDescribe::Ptr select(const std::string& name)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                const auto it = _services.find(name);
                return it == _services.end() ? ServiceDescribe::Ptr() : it->second;
            }

            void remove(const std::string& name)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _services.erase(name);
            }

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, ServiceDescribe::Ptr> _services;
        };

        class RpcRouter
        {
        public:
            using Ptr = std::shared_ptr<RpcRouter>;

            RpcRouter() : _service_manager(std::make_shared<ServiceManager>()) {}

            void registerMethod(const ServiceDescribe::Ptr& service)
            {
                _service_manager->insert(service);
            }

            // Dispatcher 负责选 RPC 入口；这里再按方法名选择具体服务。
            void onRpcRequest(const BaseConnection::Ptr& conn, RpcRequest::Ptr& request)
            {
                const auto service = _service_manager->select(request->GetMethod());
                if (!service)
                {
                    response(conn, request, Json::Value(), RCode::RCODE_NOT_FOUND_SERVICE);
                    return;
                }

                const Json::Value params = request->GetParams();
                if (!service->paramCheck(params))
                {
                    response(conn, request, Json::Value(), RCode::RCODE_INVALID_PARAMS);
                    return;
                }

                Json::Value result;
                bool called = false;
                try
                {
                    called = service->call(params, result);
                }
                catch (const std::exception& error)
                {
                    ELOG("RPC业务回调异常：%s", error.what());
                    response(conn, request, Json::Value(), RCode::RCODE_INTERNAL_ERROR);
                    return;
                }
                catch (...)
                {
                    ELOG("RPC业务回调发生未知异常！");
                    response(conn, request, Json::Value(), RCode::RCODE_INTERNAL_ERROR);
                    return;
                }

                if (!called)
                {
                    response(conn, request, Json::Value(), RCode::RCODE_INTERNAL_ERROR);
                    return;
                }
                response(conn, request, result, RCode::RCODE_OK);
            }

        private:
            void response(const BaseConnection::Ptr& conn, const RpcRequest::Ptr& request,
                          const Json::Value& result, RCode code)
            {
                auto message = MessageFactory::create<RpcResponse>();
                message->SetId(request->GetId());
                message->SetMType(MType::RSP_RPC);
                message->SetRCode(code);
                message->SetResult(result);
                conn->Send(message);
            }

            ServiceManager::Ptr _service_manager;
        };
    }
}
