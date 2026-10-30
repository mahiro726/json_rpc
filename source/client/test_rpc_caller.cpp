#include "rpc_caller.hpp"
#include "../common/dispatcher.hpp"
#include "../server/rpc_router.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

namespace
{
    using namespace lccrpc;
    using lccrpc::client::Requestor;
    using lccrpc::client::RpcCaller;

    void require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    class CaptureConnection : public BaseConnection
    {
    public:
        void Send(const BaseMessage::Ptr& msg) override
        {
            sent.push_back(msg);
            if (on_send) on_send(msg);
        }
        void Shutdown() override { throw std::runtime_error("unexpected shutdown"); }
        bool Connected() override { return true; }

        std::vector<BaseMessage::Ptr> sent;
        std::function<void(const BaseMessage::Ptr&)> on_send;
    };

    BaseMessage::Ptr response(const std::string& id, const Json::Value& value)
    {
        auto rsp = std::make_shared<RpcResponse>();
        rsp->SetId(id);
        rsp->SetMType(MType::RSP_RPC);
        rsp->SetRCode(RCode::RCODE_OK);
        rsp->SetResult(value);
        return rsp;
    }

    BaseMessage::Ptr bad_response(const std::string& id, int kind)
    {
        if (kind == 0)
        {
            auto wrong = std::make_shared<TopicResponse>();
            wrong->SetId(id);
            wrong->SetMType(MType::RSP_RPC);
            wrong->SetRCode(RCode::RCODE_OK);
            return wrong;
        }
        auto rsp = std::make_shared<RpcResponse>();
        rsp->SetId(id);
        rsp->SetMType(MType::RSP_RPC);
        // 错误响应不需要 result；成功但缺 result 的响应无效。
        if (kind == 1) rsp->SetRCode(RCode::RCODE_NOT_FOUND_SERVICE);
        else if (kind == 2) rsp->SetRCode(RCode::RCODE_OK);
        // kind == 3 不设置状态码，检查畸形响应不能逃逸 Json 异常。
        return rsp;
    }

    Json::Value get_ready(RpcCaller::JsonAsyncResponse& future)
    {
        require(future.valid(), "missing JSON future");
        require(future.wait_for(std::chrono::seconds(2)) == std::future_status::ready,
                "JSON future was not completed");
        return future.get();
    }

    void expect_error(RpcCaller::JsonAsyncResponse& future)
    {
        require(future.wait_for(std::chrono::seconds(2)) == std::future_status::ready,
                "error response left JSON future pending");
        bool specific_error = false;
        try { future.get(); }
        catch (const std::future_error&) { throw std::runtime_error("lost error as broken_promise"); }
        catch (const std::runtime_error& e) { specific_error = std::string(e.what()).size() > 0; }
        require(specific_error, "error response did not complete with an explicit error");
    }

    void run()
    {
        auto requestor = std::make_shared<Requestor>();
        RpcCaller caller(requestor);
        auto conn = std::make_shared<CaptureConnection>();
        Dispatcher incoming;
        incoming.RegisterHandler<BaseMessage>(MType::RSP_RPC,
            std::bind(&Requestor::OnResponse, requestor,
                      std::placeholders::_1, std::placeholders::_2));
        auto deliver = [&](BaseMessage::Ptr rsp) { incoming.OnMessage(conn, rsp); };
        Json::Value params(Json::objectValue);
        params["num1"] = 7;
        params["num2"] = 8;

        // 通过真实服务描述、两端 Dispatcher 和 Router 计算 Add；连接是测试替身。
        auto router = std::make_shared<server::RpcRouter>();
        server::SDescribeFactory factory;
        factory.setMethodName("Add");
        factory.setParamsDesc("num1", server::VType::INTEGRAL);
        factory.setParamsDesc("num2", server::VType::INTEGRAL);
        factory.setReturnType(server::VType::INTEGRAL);
        factory.setCallback([](const Json::Value& p, Json::Value& out) {
            out = p["num1"].asInt() + p["num2"].asInt();
        });
        router->registerMethod(factory.build());
        Dispatcher server_dispatcher;
        server_dispatcher.RegisterHandler<RpcRequest>(MType::REQ_RPC,
            std::bind(&server::RpcRouter::onRpcRequest, router,
                      std::placeholders::_1, std::placeholders::_2));
        auto reply = std::make_shared<CaptureConnection>();
        reply->on_send = deliver;
        conn->on_send = [&](const BaseMessage::Ptr& msg) {
            auto req = std::dynamic_pointer_cast<RpcRequest>(msg);
            require(req && req->Check(), "caller did not build a valid RPC request");
            require(req->GetMType() == MType::REQ_RPC, "wrong request message type");
            require(!req->GetId().empty(), "missing request ID");
            BaseMessage::Ptr input = msg;
            server_dispatcher.OnMessage(reply, input);
        };
        Json::Value sync;
        require(caller.Call(conn, "Add", params, sync) && sync.asInt() == 15,
                "synchronous Add failed");
        RpcCaller::JsonAsyncResponse immediate;
        require(caller.Call(conn, "Add", params, immediate), "immediate async send failed");
        require(get_ready(immediate).asInt() == 15, "immediate async Add failed");
        int callback_calls = 0;
        require(caller.Call(conn, "Add", params, [&](const Json::Value& result) {
            require(result.asInt() == 15, "callback exposed the wrong JSON result");
            ++callback_calls;
        }), "callback Add send failed");
        require(callback_calls == 1, "callback Add was not delivered");
        sync = "unchanged";
        require(!caller.Call(conn, "Missing", params, sync), "missing method accepted");
        require(sync.asString() == "unchanged", "failed sync call changed output");
        Json::Value invalid_params = params;
        invalid_params["num2"] = "eight";
        require(!caller.Call(conn, "Add", invalid_params, sync), "invalid business parameters accepted");

        // 延后并逆序交付，确保 JSON promise 与某一次请求关联。
        conn->on_send = std::function<void(const BaseMessage::Ptr&)>();
        RpcCaller::JsonAsyncResponse first, second;
        require(caller.Call(conn, "Add", params, first), "first async send failed");
        auto first_req = conn->sent.back();
        require(caller.Call(conn, "Add", params, second), "second async send failed");
        auto second_req = conn->sent.back();
        require(first.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout,
                "async Call waited for or fabricated a result");
        deliver(response(second_req->GetId(), 200));
        require(get_ready(second).asInt() == 200, "second call received wrong response");
        require(first.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout,
                "second response completed first call");
        deliver(response(first_req->GetId(), 100));
        require(get_ready(first).asInt() == 100, "first call received wrong response");
        auto captured = std::dynamic_pointer_cast<RpcRequest>(first_req);
        require(captured->GetMethod() == "Add" && captured->GetParams() == params,
                "caller lost method or parameters");

        std::set<std::string> ids;
        for (const auto& sent : conn->sent)
            require(ids.insert(sent->GetId()).second, "caller reused a request ID");

        // 每一种错误分别验证同步返回、future 异常和回调抑制。
        for (int kind = 0; kind < 4; ++kind)
        {
            conn->on_send = [&](const BaseMessage::Ptr& msg) {
                deliver(bad_response(msg->GetId(), kind));
            };
            sync = 1234;
            require(!caller.Call(conn, "Add", params, sync), "invalid sync response accepted");
            require(sync.asInt() == 1234, "invalid sync response changed result");
            RpcCaller::JsonAsyncResponse failed;
            require(caller.Call(conn, "Add", params, failed), "async initiation unexpectedly failed");
            expect_error(failed);
            bool called = false;
            require(caller.Call(conn, "Add", params, [&](const Json::Value&) { called = true; }),
                    "callback initiation unexpectedly failed");
            require(!called, "invalid response reached user callback");
        }

        // Call 返回甚至 RpcCaller 销毁后，适配器与 promise 仍能正常工作。
        conn->on_send = std::function<void(const BaseMessage::Ptr&)>();
        RpcCaller::JsonAsyncResponse survives;
        {
            RpcCaller temporary(requestor);
            require(temporary.Call(conn, "Add", params, survives), "temporary caller send failed");
        }
        deliver(response(conn->sent.back()->GetId(), 77));
        require(get_ready(survives).asInt() == 77, "promise did not outlive Call");
        int delayed_calls = 0;
        {
            RpcCaller temporary(requestor);
            require(temporary.Call(conn, "Add", params, [&](const Json::Value& v) {
                delayed_calls += v.asInt();
            }), "temporary callback caller send failed");
        }
        auto delayed = response(conn->sent.back()->GetId(), 1);
        deliver(delayed);
        deliver(delayed);
        require(delayed_calls == 1, "delayed callback lifetime or cleanup failed");

        // 用户回调抛异常后，也应返回到 Requestor 完成描述清理。
        require(caller.Call(conn, "Add", params, [](const Json::Value&) {
            throw std::runtime_error("user callback failure");
        }), "throwing callback send failed");
        std::weak_ptr<BaseMessage> held = conn->sent.back();
        auto throwing = response(conn->sent.back()->GetId(), 15);
        conn->sent.clear();
        deliver(throwing);
        require(held.expired(), "throwing callback prevented description cleanup");

        const auto before = conn->sent.size();
        require(!caller.Call(conn, "Add", params, RpcCaller::JsonResponseCallback()),
                "empty callback accepted");
        RpcCaller::JsonAsyncResponse no_future;
        require(!caller.Call(BaseConnection::Ptr(), "Add", params, sync), "null connection accepted");
        require(!caller.Call(BaseConnection::Ptr(), "Add", params, no_future), "null async connection accepted");
        RpcCaller invalid{Requestor::Ptr()};
        require(!invalid.Call(conn, "Add", params, sync), "null requestor accepted");
        require(conn->sent.size() == before, "invalid call still sent a request");
    }
}

int main()
{
    try
    {
        run();
        std::cout << "RpcCaller tests passed\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr << "RpcCaller tests failed: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
