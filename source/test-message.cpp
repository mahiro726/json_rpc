#include "message.hpp"


int main()
{
    // lccrpc::RpcRequest::Ptr rrp = lccrpc::MessageFactory::create<lccrpc::RpcRequest>();

    // Json::Value param;
    // param["num1"] = 11;
    // param["num2"] = 22;

    // rrp->SetMethod("Add"); // 方法名称
    // rrp->SetParams(param); // 参数字段
    // std::string str = rrp->Serialize();

    // lccrpc::BaseMessage::Ptr bmp = lccrpc::MessageFactory::create(lccrpc::MType::REQ_RPC);
    // bool ret = bmp->Deserialize(str);
    // if (ret == false)
    // {
    //     return -1;
    // }
    // ret = bmp->Check();
    // if (ret == false)
    // {
    //     return -2;
    // }

    // lccrpc::RpcRequest::Ptr rrp2 = std::dynamic_pointer_cast<lccrpc::RpcRequest>(bmp);
    // std::cout << rrp2->GetMethod() << std::endl;
    // std::cout << rrp2->GetParems()["num1"].asInt() << std::endl;
    // std::cout << rrp2->GetParems()["num2"].asInt() << std::endl;

    lccrpc::TopicRequest::Ptr trp = lccrpc::MessageFactory::create<lccrpc::TopicRequest>();

    trp->SetTopicKey("news");
    trp->SetTopicOptype(lccrpc::TopicOptype::TOPIC_PUBLISH);
    trp->SetTopicMessage("hello world");

    std::string str = trp->Serialize();

    lccrpc::TopicRequest::Ptr trp2 = lccrpc::MessageFactory::create<lccrpc::TopicRequest>();
    bool ret = trp2->Deserialize(str);
    if (ret == false)
    {
        return -1;
    }
    ret = trp2->Check();
    if (ret == false)
    {
        return -2;
    }

    std::cout << trp2->GetTopicKey() << std::endl;
    std::cout << (int)trp2->GetTopicOptype() << std::endl;
    std::cout << trp2->GetTopicMessage() << std::endl;
}