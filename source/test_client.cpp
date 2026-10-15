#include "message.hpp"
#include "net.hpp"
#include "dispatcher.hpp"
#include "fields.hpp"

void onRpcResponse(const lccrpc::BaseConnection::Ptr& conn, lccrpc::BaseMessage::Ptr& msg)
{
    std::cout << "收到了RPC应答：";
    std::string body = msg->Serialize();
    std::cout << body << std::endl;
}

void onTopicResponse(const lccrpc::BaseConnection::Ptr& conn, lccrpc::BaseMessage::Ptr& msg)
{
    std::cout << "收到了Topic应答：";
    std::string body = msg->Serialize();
    std::cout << body << std::endl;
}

int main()
{
    auto dispatcher = std::make_shared<lccrpc::Dispatcher>();
    //注册映射关系
    dispatcher->RegisterHandler(lccrpc::MType::RSP_RPC, onRpcResponse);
    dispatcher->RegisterHandler(lccrpc::MType::RSP_TOPIC, onTopicResponse);
    
    auto client = lccrpc::ClientFactory::create("127.0.0.1", 8080);

    auto message_callback = std::bind(&lccrpc::Dispatcher::OnMessage, dispatcher.get(), std::placeholders::_1, std::placeholders::_2);

    client->SetMessageCallback(message_callback);
    client->Connect();
    auto rpc_req = lccrpc::MessageFactory::create<lccrpc::RpcRequest>();
    rpc_req->SetId("1111");
    rpc_req->SetMType(lccrpc::MType::REQ_RPC);
    rpc_req->SetMethod("Add");
    
    Json::Value param;
    param["num1"] = 11;
    param["num2"] = 22;
    rpc_req->SetParams(param);

    client->Send(rpc_req);


    auto topic_req = lccrpc::MessageFactory::create<lccrpc::TopicRequest>();
    topic_req->SetId("1112");
    topic_req->SetMType(lccrpc::MType::REQ_TOPIC);
    topic_req->SetTopicKey("news");
    topic_req->SetTopicOptype(lccrpc::TopicOptype::TOPIC_SUBSCRIBE);
    client->Send(topic_req);

    client->Shutdown();
}