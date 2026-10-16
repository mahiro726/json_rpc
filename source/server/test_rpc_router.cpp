#include "rpc_router.hpp"
#include "../common/dispatcher.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    class CaptureConnection : public lccrpc::BaseConnection
    {
    public:
        void Send(const lccrpc::BaseMessage::Ptr& message) override
        {
            // 经过真实序列化／反序列化，检查接收端能够接受响应。
            auto response = std::make_shared<lccrpc::RpcResponse>();
            require(response->Deserialize(message->Serialize()), "response decoding failed");
            require(response->Check(), "response validation failed");
            response->SetId(message->GetId());
            response->SetMType(message->GetMType());
            last = response;
            ++sent;
        }
        void Shutdown() override {}
        bool Connected() override { return true; }

        lccrpc::RpcResponse::Ptr last;
        unsigned sent = 0;
    };

    using namespace lccrpc;
    using namespace lccrpc::server;

    ServiceDescribe::Ptr describe(const std::string& name,
                                 const ServiceDescribe::ServiceCallback& callback,
                                 VType result_type = VType::INTEGRAL)
    {
        SDescribeFactory factory;
        factory.setMethodName(name);
        factory.setParamsDesc("num1", VType::INTEGRAL);
        factory.setParamsDesc("num2", VType::INTEGRAL);
        factory.setReturnType(result_type);
        factory.setCallback(callback);
        return factory.build();
    }

    RpcResponse::Ptr invoke(Dispatcher& dispatcher, const std::shared_ptr<CaptureConnection>& conn,
                           const std::string& method, const Json::Value& params,
                           const std::string& id)
    {
        auto request = std::make_shared<RpcRequest>();
        request->SetId(id);
        request->SetMType(MType::REQ_RPC);
        request->SetMethod(method);
        request->SetParams(params);
        require(request->GetParems() == request->GetParams(), "legacy accessor changed");
        const unsigned before = conn->sent;
        BaseMessage::Ptr incoming = request;
        dispatcher.OnMessage(conn, incoming);
        require(conn->sent == before + 1, "request must produce exactly one response");
        require(conn->last->GetId() == id, "response must preserve request ID");
        require(conn->last->GetMType() == MType::RSP_RPC, "incorrect response message type");
        return conn->last;
    }

    void run()
    {
        auto router = std::make_shared<RpcRouter>();
        Dispatcher dispatcher;
        dispatcher.RegisterHandler<RpcRequest>(MType::REQ_RPC,
            std::bind(&RpcRouter::onRpcRequest, router,
                      std::placeholders::_1, std::placeholders::_2));
        auto conn = std::make_shared<CaptureConnection>();
        int calls = 0;
        auto add = describe("Add", [&calls](const Json::Value& params, Json::Value& result) {
            ++calls;
            result = params["num1"].asInt() + params["num2"].asInt();
        });
        router->registerMethod(add);

        Json::Value params(Json::objectValue);
        params["num1"] = 7;
        params["num2"] = 8;
        auto response = invoke(dispatcher, conn, "Add", params, "request-42");
        require(response->GetRCode() == RCode::RCODE_OK, "Add failed");
        require(response->GetResult().asInt() == 15, "result must be computed, not fixed at 33");
        require(calls == 1, "successful Add must execute once");

        response = invoke(dispatcher, conn, "Missing", params, "missing-43");
        require(response->GetRCode() == RCode::RCODE_NOT_FOUND_SERVICE, "wrong missing-service code");
        require(response->GetResult().isNull(), "error response must not fabricate a result");
        require(calls == 1, "unknown method executed a callback");

        Json::Value missing(Json::objectValue);
        missing["num1"] = 7;
        response = invoke(dispatcher, conn, "Add", missing, "missing-param-44");
        require(response->GetRCode() == RCode::RCODE_INVALID_PARAMS, "missing parameter accepted");
        params["num2"] = "eight";
        response = invoke(dispatcher, conn, "Add", params, "wrong-type-45");
        require(response->GetRCode() == RCode::RCODE_INVALID_PARAMS, "wrong parameter type accepted");
        response = invoke(dispatcher, conn, "Add", Json::Value(7), "not-object-46");
        require(response->GetRCode() == RCode::RCODE_INVALID_PARAMS, "non-object parameters accepted");
        require(calls == 1, "invalid parameters reached the business callback");

        params["num2"] = 8;
        router->registerMethod(describe("BadResult", [](const Json::Value&, Json::Value& result) {
            result = "not an integer";
        }));
        response = invoke(dispatcher, conn, "BadResult", params, "bad-result-47");
        require(response->GetRCode() == RCode::RCODE_INTERNAL_ERROR, "wrong result type accepted");

        router->registerMethod(describe("Throws", [](const Json::Value&, Json::Value&) {
            throw std::runtime_error("business error");
        }));
        response = invoke(dispatcher, conn, "Throws", params, "exception-48");
        require(response->GetRCode() == RCode::RCODE_INTERNAL_ERROR, "callback exception escaped");

        // 同名插入保留最初的 Add，不悄悄替换为另一项服务。
        router->registerMethod(describe("Add", [](const Json::Value&, Json::Value& result) {
            result = 999;
        }));
        response = invoke(dispatcher, conn, "Add", params, "duplicate-49");
        require(response->GetResult().asInt() == 15, "duplicate registration replaced original service");

        // select 取得的共享指针在移除映射后仍能保活当前调用。
        ServiceManager manager;
        manager.insert(add);
        auto selected = manager.select("Add");
        require(selected == add, "manager lookup failed");
        manager.remove("Add");
        require(!manager.select("Add"), "manager removal failed");
        Json::Value result;
        require(selected->call(params, result) && result.asInt() == 15,
                "selected service did not survive removal");

        // 六种类型要求、数值类别兼容整数与浮点，以及零参数方法。
        SDescribeFactory typed;
        typed.setMethodName("Types");
        typed.setParamsDesc("flag", VType::BOOL);
        typed.setParamsDesc("count", VType::INTEGRAL);
        typed.setParamsDesc("number", VType::NUMERIC);
        typed.setParamsDesc("text", VType::STRING);
        typed.setParamsDesc("items", VType::ARRAY);
        typed.setParamsDesc("object", VType::OBJECT);
        typed.setReturnType(VType::BOOL);
        typed.setCallback([](const Json::Value&, Json::Value& output) { output = true; });
        auto types = typed.build();
        Json::Value values(Json::objectValue);
        values["flag"] = true;
        values["count"] = 3;
        values["number"] = 2.5;
        values["text"] = "hello";
        values["items"] = Json::Value(Json::arrayValue);
        values["object"] = Json::Value(Json::objectValue);
        require(types->paramCheck(values), "valid type descriptions rejected");
        values["number"] = 2;
        require(types->paramCheck(values), "NUMERIC incorrectly rejected an integer");
        require(types->call(values, result) && result.asBool(), "boolean result rejected");

        SDescribeFactory empty;
        empty.setMethodName("Ping");
        empty.setReturnType(VType::STRING);
        empty.setCallback([](const Json::Value&, Json::Value& output) { output = "pong"; });
        auto ping = empty.build();
        require(ping->paramCheck(Json::Value(Json::objectValue)), "zero-parameter method rejected");
        require(ping->call(Json::Value(Json::objectValue), result) && result.asString() == "pong",
                "zero-parameter method failed");

        bool rejected = false;
        try { SDescribeFactory invalid; invalid.build(); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "incomplete service configuration accepted");

        RpcResponse invalid_response;
        invalid_response.SetRCode(RCode::RCODE_OK);
        require(!invalid_response.Check(), "successful response without result accepted");
    }
}

int main()
{
    try
    {
        run();
        std::cout << "RpcRouter regression checks passed\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "RpcRouter regression failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
