#pragma once

#include "abstract.hpp"
#include "fields.hpp"
#include "detail.hpp"

namespace lccrpc
{
    typedef std::pair<std::string, int> Address;

    class JsonMessage : public BaseMessage
    {
    public:
        using Ptr = std::shared_ptr<JsonMessage>;
        virtual std::string Serialize() override
        {
            std::string msg;
            bool ret = JsonUtil::Serialize(_body, msg); // 复用已经写过的序列化函数
            if (ret == false)
            {
                return std::string();
            }
            return msg;
        }
        virtual bool Deserialize(const std::string msg) override
        {
            return JsonUtil::Deserialze(msg, _body);
        }

    protected:
        Json::Value _body;
    };

    class JsonRequest : public JsonMessage
    {
    public:
        using Ptr = std::shared_ptr<JsonRequest>;
    };

    class JsonResponse : public JsonMessage
    {
    public:
        using Ptr = std::shared_ptr<JsonResponse>;
        virtual bool Check() override
        {
            // 在响应中，大部分的响应都只有响应状态码
            // 因此只需要判断响应状态码字段是否存在，类型是否正确即可

            // 判断状态码是否存在以及类型
            if (_body[KEY_RCODE].isNull() == true || _body[KEY_RCODE].isIntegral() == false)
            {
                ELOG("响应中没有响应状态码 或者 状态码类型错误！");
                return false;
            }

            return true;
        }

        virtual RCode GetRCode()
        {
            return (RCode)_body[KEY_RCODE].asInt();
        }
        virtual void SetRCode(RCode rcode)
        {
            _body[KEY_RCODE] = (int)rcode;
        }
    };

    class RpcRequest : public JsonRequest
    {
    public:
        using Ptr = std::shared_ptr<RpcRequest>;
        virtual bool Check() override
        {
            // rpc请求中，包含请求方法名称——字符串，参数字段——对象

            // 判断方法类型和方法名称
            if (_body[KEY_METHOD].isNull() == true || _body[KEY_METHOD].isString() == false)
            {
                ELOG("RPC请求中没有方法名称 或者 方法类型错误！");
                return false;
            }

            // 判断参数存在以及类型
            if ((_body[KEY_PARAMS].isNull() == true) || (_body[KEY_PARAMS].isObject() == false))
            {
                ELOG("RPC请求中参数信息不存在 或者 参数信息类型错误！");
                return false;
            }
            return true;
        }

        std::string GetMethod()
        {
            return _body[KEY_METHOD].asString();
        }
        void SetMethod(const std::string &method_name)
        {
            _body[KEY_METHOD] = method_name;
        }

        Json::Value GetParams()
        {
            return _body[KEY_PARAMS];
        }

        // 保留旧拼写，兼容已经写过的调用代码。
        Json::Value GetParems()
        {
            return GetParams();
        }
        void SetParams(const Json::Value &params)
        {
            _body[KEY_PARAMS] = params;
        }
    };

    class TopicRequest : public JsonRequest
    {
    public:
        using Ptr = std::shared_ptr<TopicRequest>;
        virtual bool Check() override
        {
            if (_body[KEY_TOPIC_KEY].isNull() == true || _body[KEY_TOPIC_KEY].isString() == false)
            {
                ELOG("主题请求中没有主题名称 或者 主题类型错误！");
                return false;
            }

            if ((_body[KEY_OPTYPE].isNull() == true) || (_body[KEY_OPTYPE].isIntegral() == false))
            {
                ELOG("主题请求中 操作类型不存在 或者 操作类型的类型错误！");
                return false;
            }

            if (_body[KEY_OPTYPE].asInt() == (int)TopicOptype::TOPIC_PUBLISH &&
                (_body[KEY_TOPIC_MSG].isNull() == true || _body[KEY_TOPIC_MSG].isString() == false))
            {
                ELOG("主题消息发布请求中 没有消息内容字段 或 消息内容类型错误！");
                return false;
            }
            return true;
        }

        std::string GetTopicKey()
        {
            return _body[KEY_TOPIC_KEY].asString();
        }
        void SetTopicKey(const std::string &key)
        {
            _body[KEY_TOPIC_KEY] = key;
        }

        TopicOptype GetTopicOptype()
        {
            return (TopicOptype)_body[KEY_OPTYPE].asInt();
        }
        void SetTopicOptype(const TopicOptype &type)
        {
            _body[KEY_OPTYPE] = (int)type;
        }

        std::string GetTopicMessage()
        {
            return _body[KEY_TOPIC_MSG].asString();
        }
        void SetTopicMessage(const std::string &msg)
        {
            _body[KEY_TOPIC_MSG] = msg;
        }
    };

    class ServiceRequest : public JsonRequest
    {
    public:
        using Ptr = std::shared_ptr<ServiceRequest>;

        virtual bool Check() override
        {
            // 方法名称：必须存在，且为字符串
            if (_body[KEY_METHOD].isNull() == true ||
                _body[KEY_METHOD].isString() == false)
            {
                ELOG("服务请求中没有方法名称 或者 方法名称类型错误！");
                return false;
            }

            // 操作类型：必须存在，且为整数
            if (_body[KEY_OPTYPE].isNull() == true ||
                _body[KEY_OPTYPE].isIntegral() == false)
            {
                ELOG("服务请求中操作类型不存在 或者 操作类型的类型错误！");
                return false;
            }

            // 服务发现不需要 host；其他操作需要检查主机信息
            if (_body[KEY_OPTYPE].asInt() != (int)ServiceOptype::SERVICE_DISCOVERY &&
                (_body[KEY_HOST].isNull() == true ||
                 _body[KEY_HOST].isObject() == false ||
                 _body[KEY_HOST][KEY_HOST_IP].isNull() == true ||
                 _body[KEY_HOST][KEY_HOST_IP].isString() == false ||
                 _body[KEY_HOST][KEY_HOST_PORT].isNull() == true ||
                 _body[KEY_HOST][KEY_HOST_PORT].isIntegral() == false))
            {
                ELOG("服务请求中主机地址信息错误！");
                return false;
            }

            return true;
        }

        std::string GetMethod()
        {
            return _body[KEY_METHOD].asString();
        }

        void SetMethod(const std::string &method_name)
        {
            _body[KEY_METHOD] = method_name;
        }

        ServiceOptype GetServiceOptype()
        {
            return (ServiceOptype)_body[KEY_OPTYPE].asInt();
        }

        void SetServiceOptype(const ServiceOptype &type)
        {
            _body[KEY_OPTYPE] = (int)type;
        }

        Address GetHost()
        {
            Address addr;
            addr.first = _body[KEY_HOST][KEY_HOST_IP].asString();
            addr.second = _body[KEY_HOST][KEY_HOST_PORT].asInt();
            return addr;
        }

        void SetHost(const Address &host)
        {
            Json::Value value;
            value[KEY_HOST_IP] = host.first;
            value[KEY_HOST_PORT] = host.second;
            _body[KEY_HOST] = value;
        }
    };

    class RpcResponse : public JsonResponse
    {
    public:
        using Ptr = std::shared_ptr<RpcResponse>;

        virtual bool Check() override
        {
            // 先检查所有响应共有的状态码
            if (JsonResponse::Check() == false)
            {
                return false;
            }

            // 错误响应可以没有业务结果；成功响应仍要求结果非空。
            if (GetRCode() == RCode::RCODE_OK && _body[KEY_RESULT].isNull())
            {
                ELOG("RPC响应中没有调用结果！");
                return false;
            }
            return true;
        }

        Json::Value GetResult()
        {
            return _body[KEY_RESULT];
        }

        void SetResult(const Json::Value &result)
        {
            _body[KEY_RESULT] = result;
        }
    };

    class TopicResponse : public JsonResponse
    {
    public:
        using Ptr = std::shared_ptr<TopicResponse>;

        // 只需要响应状态码，直接继承父类的检查和存取接口
    };

    class ServiceResponse : public JsonResponse
    {
    public:
        using Ptr = std::shared_ptr<ServiceResponse>;

        virtual bool Check() override
        {
            if (JsonResponse::Check() == false)
            {
                return false;
            }

            // 用操作类型区分注册、发现、上线和下线响应
            if (_body[KEY_OPTYPE].isNull() == true ||
                _body[KEY_OPTYPE].isIntegral() == false)
            {
                ELOG("服务响应中没有操作类型 或者 操作类型的类型错误！");
                return false;
            }

            // 服务发现响应还需要方法名称和主机列表
            if (_body[KEY_OPTYPE].asInt() == (int)ServiceOptype::SERVICE_DISCOVERY &&
                (_body[KEY_METHOD].isNull() == true ||
                 _body[KEY_METHOD].isString() == false ||
                 _body[KEY_HOST].isNull() == true ||
                 _body[KEY_HOST].isArray() == false))
            {
                ELOG("服务发现响应中方法名称或主机列表信息错误！");
                return false;
            }

            return true;
        }

        ServiceOptype GetServiceOptype()
        {
            return (ServiceOptype)_body[KEY_OPTYPE].asInt();
        }

        void SetServiceOptype(const ServiceOptype &type)
        {
            _body[KEY_OPTYPE] = (int)type;
        }

        std::string GetMethod()
        {
            return _body[KEY_METHOD].asString();
        }

        void SetMethod(const std::string &method_name)
        {
            _body[KEY_METHOD] = method_name;
        }

        void SetHosts(const std::vector<Address> &hosts)
        {
            // 先设为空数组，空列表也能保持正确类型；
            // 再次设置时替换旧列表
            _body[KEY_HOST] = Json::Value(Json::arrayValue);

            for (const auto &host : hosts)
            {
                Json::Value value;
                value[KEY_HOST_IP] = host.first;
                value[KEY_HOST_PORT] = host.second;
                _body[KEY_HOST].append(value);
            }
        }

        std::vector<Address> GetHosts()
        {
            std::vector<Address> hosts;

            for (Json::ArrayIndex i = 0; i < _body[KEY_HOST].size(); ++i)
            {
                Address addr;
                addr.first = _body[KEY_HOST][i][KEY_HOST_IP].asString();
                addr.second = _body[KEY_HOST][i][KEY_HOST_PORT].asInt();
                hosts.push_back(addr);
            }

            return hosts;
        }
    };

    // 实现一个消息对象的生产工厂
    class MessageFactory
    {
    public:
        static BaseMessage::Ptr create(MType mtype)
        {
            switch (mtype)
            {
            case MType::REQ_RPC:
                return std::make_shared<RpcRequest>();
            case MType::RSP_RPC:
                return std::make_shared<RpcResponse>();
            case MType::REQ_TOPIC:
                return std::make_shared<TopicRequest>();
            case MType::RSP_TOPIC:
                return std::make_shared<TopicResponse>();
            case MType::REQ_SERVICE:
                return std::make_shared<ServiceRequest>();
            case MType::RSP_SERVICE:
                return std::make_shared<ServiceResponse>();
            }
            return BaseMessage::Ptr();
        }

        template <typename T, typename... Args>
        static std::shared_ptr<T> create(Args &&...args)
        {
            return std::make_shared<T>(std::forward<Args>(args)...);
        }
    };
}
