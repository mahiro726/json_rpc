#include "message.hpp"
#include "net.hpp"
#include "dispatcher.hpp"

void onRpcRequest(const lccrpc::BaseConnection::Ptr& conn, lccrpc::RpcRequest::Ptr& msg)
{
    std::cout << "调用方法：" << msg->GetMethod() << std::endl;
    std::cout << "服务端收到了rpc请求：";
    std::string body = msg->Serialize();
    std::cout << body << std::endl;
    auto rpc_res = lccrpc::MessageFactory::create<lccrpc::RpcResponse>();
    rpc_res->SetId("1111");
    rpc_res->SetMType(lccrpc::MType::RSP_RPC);
    rpc_res->SetRCode(lccrpc::RCode::RCODE_OK);
    rpc_res->SetResult(33);
    conn->Send(rpc_res);
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
    auto dispatcher = std::make_shared<lccrpc::Dispatcher>();
    // 注册对应的消息处理函数
    dispatcher->RegisterHandler<lccrpc::RpcRequest>(lccrpc::MType::REQ_RPC, onRpcRequest);
    dispatcher->RegisterHandler<lccrpc::TopicRequest>(lccrpc::MType::REQ_TOPIC, onTopicRequest);

    // 有了bind就可以把成员函数注册进去了
    auto message_callback = std::bind(&lccrpc::Dispatcher::OnMessage, dispatcher.get(), std::placeholders::_1, std::placeholders::_2);

    auto server = lccrpc::ServerFactory::create(8080);
    server->SetMessageCallback(message_callback);
    server->Start();
}