#include "../common/message.hpp"
#include "../common/net.hpp"
#include "../common/dispatcher.hpp"
#include "../common/fields.hpp"
#include "../client/rpc_caller.hpp"
#include "../client/requestor.hpp"

void onRpcResponse(const lccrpc::BaseConnection::Ptr& conn, lccrpc::RpcResponse::Ptr& msg)
{
    std::cout << "收到了RPC应答：";
    std::string body = msg->Serialize();
    std::cout << body << std::endl;
}

void onTopicResponse(const lccrpc::BaseConnection::Ptr& conn, lccrpc::TopicResponse::Ptr& msg)
{
    std::cout << "收到了Topic应答：";
    std::string body = msg->Serialize();
    std::cout << body << std::endl;
}

int main()
{
    auto requestor = std::make_shared<lccrpc::client::Requestor>();
    auto caller = std::make_shared<lccrpc::client::RpcCaller>(requestor);

    auto dispatcher = std::make_shared<lccrpc::Dispatcher>();

    auto response_cb = std::bind(&lccrpc::client::Requestor::OnResponse, requestor, std::placeholders::_1, std::placeholders::_2);

    //注册映射关系
    dispatcher->RegisterHandler<lccrpc::BaseMessage>(lccrpc::MType::RSP_RPC, response_cb);
    dispatcher->RegisterHandler<lccrpc::TopicResponse>(lccrpc::MType::RSP_TOPIC, onTopicResponse);

    auto client = lccrpc::ClientFactory::create("127.0.0.1", 8080);

    auto message_callback = std::bind(&lccrpc::Dispatcher::OnMessage, dispatcher.get(), std::placeholders::_1, std::placeholders::_2);

    client->SetMessageCallback(message_callback);

//--------------------------------------------以上都是模块配置和注册-----------------------------------------------
    client->Connect();
    auto conn = client->Connection();

    auto rpc_req = lccrpc::MessageFactory::create<lccrpc::RpcRequest>();
    rpc_req->SetId("1111");
    rpc_req->SetMType(lccrpc::MType::REQ_RPC);
    rpc_req->SetMethod("Add");

    Json::Value param;
    param["num1"] = 11;
    param["num2"] = 22;

    Json::Value result;
    bool ret = caller->Call(conn, "Add", param, result);

    if (ret)
    {
        std::cout << "result: " << result.asInt() << std::endl;
    }

    // topic请求
    // auto topic_req = lccrpc::MessageFactory::create<lccrpc::TopicRequest>();
    // topic_req->SetId("1112");
    // topic_req->SetMType(lccrpc::MType::REQ_TOPIC);
    // topic_req->SetTopicKey("news");
    // topic_req->SetTopicOptype(lccrpc::TopicOptype::TOPIC_SUBSCRIBE);
    // client->Send(topic_req);

    // // 发送函数返回不代表服务端已经处理完成，也不代表客户端已经收到响应
    // std::cout << "等待服务器响应，看到两条应答后按回车退出……" << std::endl;
    // std::cin.get();

    client->Shutdown();
}