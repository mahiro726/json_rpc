#include "../common/message.hpp"
#include "../common/net.hpp"
#include "../common/dispatcher.hpp"
#include "../server/rpc_router.hpp"

void Add(const Json::Value& req, Json::Value& rsp)
{
    int num1 = req["num1"].asInt();
    int num2 = req["num2"].asInt();
    rsp = num1 + num2;
}

void onTopicRequest(const lccrpc::BaseConnection::Ptr& conn, lccrpc::TopicRequest::Ptr& msg)
{
    std::cout << "服务端收到了topic请求：";
    std::string body = msg->Serialize();
    std::cout << body << std::endl;
    auto rpc_res = lccrpc::MessageFactory::create<lccrpc::TopicResponse>();
    rpc_res->SetId("1112");
    rpc_res->SetMType(lccrpc::MType::RSP_TOPIC);
    rpc_res->SetRCode(lccrpc::RCode::RCODE_OK);
    conn->Send(rpc_res);
}

int main()
{
    auto router = std::make_shared<lccrpc::server::RpcRouter>();
    std::unique_ptr<lccrpc::server::SDescribeFactory> desc_factory(new lccrpc::server::SDescribeFactory());

    // set方法描述，并注册到router管理的服务表中
    desc_factory->setMethodName("Add");
    desc_factory->setParamsDesc("num1", lccrpc::server::VType::INTEGRAL);
    desc_factory->setParamsDesc("num2", lccrpc::server::VType::INTEGRAL);
    desc_factory->setReturnType(lccrpc::server::VType::INTEGRAL);
    desc_factory->setCallback(Add);
    router->registerMethod(desc_factory->build());

    auto dispatcher = std::make_shared<lccrpc::Dispatcher>();

    // 注册对应的消息处理函数
    auto rpc_callback = std::bind(&lccrpc::server::RpcRouter::onRpcRequest,
        router, std::placeholders::_1, std::placeholders::_2);
    dispatcher->RegisterHandler<lccrpc::RpcRequest>(lccrpc::MType::REQ_RPC, rpc_callback);
    dispatcher->RegisterHandler<lccrpc::TopicRequest>(lccrpc::MType::REQ_TOPIC, onTopicRequest);

    // 有了bind就可以把成员函数注册进去了
    auto message_callback = std::bind(&lccrpc::Dispatcher::OnMessage, dispatcher.get(), std::placeholders::_1, std::placeholders::_2);

    auto server = lccrpc::ServerFactory::create(8080);
    server->SetMessageCallback(message_callback);
    server->Start();
}
