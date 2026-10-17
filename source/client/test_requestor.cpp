#include "requestor.hpp"
#include "../common/dispatcher.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{
    using namespace lccrpc;
    using lccrpc::client::Requestor;

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

    BaseMessage::Ptr request(const std::string& id)
    {
        auto req = std::make_shared<RpcRequest>();
        req->SetId(id);
        req->SetMType(MType::REQ_RPC);
        req->SetMethod("Add");
        req->SetParams(Json::Value(Json::objectValue));
        return req;
    }

    BaseMessage::Ptr response(const std::string& id, int result)
    {
        auto rsp = std::make_shared<RpcResponse>();
        rsp->SetId(id);
        rsp->SetMType(MType::RSP_RPC);
        rsp->SetRCode(RCode::RCODE_OK);
        rsp->SetResult(result);
        return rsp;
    }

    void run()
    {
        auto requestor = std::make_shared<Requestor>();
        auto conn = std::make_shared<CaptureConnection>();
        Dispatcher dispatcher;
        dispatcher.RegisterHandler<BaseMessage>(MType::RSP_RPC,
            std::bind(&Requestor::OnResponse, requestor,
                      std::placeholders::_1, std::placeholders::_2));

        // 两次请求逆序返回，按 ID 配对，不按响应抵达顺序配对。
        auto req1 = request("first");
        auto req2 = request("second");
        Requestor::AsyncResponse first, second;
        require(requestor->Send(conn, req1, first), "first send failed");
        require(requestor->Send(conn, req2, second), "second send failed");
        require(conn->sent.size() == 2 && conn->sent[0] == req1 && conn->sent[1] == req2,
                "requests did not reach the connection");
        require(first.valid() && second.valid(), "send did not return valid futures");
        require(first.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout,
                "future became ready before a response");
        auto rsp2 = response("second", 22);
        dispatcher.OnMessage(conn, rsp2);
        require(second.wait_for(std::chrono::seconds(1)) == std::future_status::ready,
                "second response not delivered");
        require(second.get() == rsp2, "wrong full response delivered to second call");
        require(first.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout,
                "second response incorrectly completed first call");
        auto rsp1 = response("first", 11);
        dispatcher.OnMessage(conn, rsp1);
        require(first.wait_for(std::chrono::seconds(1)) == std::future_status::ready,
                "first response not delivered");
        require(first.get() == rsp1, "wrong full response delivered to first call");
        // 已完成请求的顺序重复响应，以及未知 ID，都不能再次交付。
        dispatcher.OnMessage(conn, rsp1);
        auto unknown = response("unknown", 0);
        dispatcher.OnMessage(conn, unknown);

        int calls = 0;
        BaseMessage::Ptr received;
        std::weak_ptr<BaseMessage> completed_request;
        {
            auto req = request("callback");
            completed_request = req;
            require(requestor->Send(conn, req, [&calls, &received](const BaseMessage::Ptr& rsp) {
                ++calls;
                received = rsp;
            }), "callback send failed");
        }
        conn->sent.clear();
        require(!completed_request.expired(), "pending request was not retained");
        auto callback_rsp = response("callback", 33);
        dispatcher.OnMessage(conn, callback_rsp);
        require(calls == 1 && received == callback_rsp, "callback received wrong response");
        require(completed_request.expired(), "completed description was not removed");
        dispatcher.OnMessage(conn, callback_rsp);
        require(calls == 1, "duplicate response called callback twice");

        // 空回调也能完成清理，不抛 bad_function_call。
        std::weak_ptr<BaseMessage> empty_request;
        {
            auto req = request("empty-callback");
            empty_request = req;
            require(requestor->Send(conn, req, Requestor::RequestCallback()), "empty callback send failed");
        }
        conn->sent.clear();
        auto empty_rsp = response("empty-callback", 0);
        dispatcher.OnMessage(conn, empty_rsp);
        require(empty_request.expired(), "empty callback description was not removed");

        // Send 内立即收到响应，先 set_value 再 get_future 仍能取得结果。
        auto immediate = response("immediate", 44);
        conn->on_send = [&](const BaseMessage::Ptr&) { dispatcher.OnMessage(conn, immediate); };
        Requestor::AsyncResponse immediate_future;
        require(requestor->Send(conn, request("immediate"), immediate_future), "immediate send failed");
        require(immediate_future.wait_for(std::chrono::seconds(1)) == std::future_status::ready,
                "immediate response was lost");
        require(immediate_future.get() == immediate, "wrong immediate response");

        // 同步重载等待另一条线程中的接收处理；超时保护避免测试永久挂起。
        conn->on_send = Requestor::RequestCallback();
        auto sync_req = request("sync");
        auto sync_rsp = response("sync", 55);
        std::promise<void> sent_signal;
        auto sent_future = sent_signal.get_future();
        conn->on_send = [&](const BaseMessage::Ptr&) { sent_signal.set_value(); };
        std::promise<BaseMessage::Ptr> finished;
        auto finished_future = finished.get_future();
        std::thread sender([&] {
            try
            {
                BaseMessage::Ptr rsp;
                require(requestor->Send(conn, sync_req, rsp), "sync send failed");
                finished.set_value(rsp);
            }
            catch (...) { finished.set_exception(std::current_exception()); }
        });
        if (sent_future.wait_for(std::chrono::seconds(3)) != std::future_status::ready)
            std::abort();
        const bool was_waiting = finished_future.wait_for(std::chrono::milliseconds(0))
                                 == std::future_status::timeout;
        dispatcher.OnMessage(conn, sync_rsp);
        if (finished_future.wait_for(std::chrono::seconds(3)) != std::future_status::ready)
            std::abort();
        sender.join();
        require(was_waiting, "sync send returned before its response");
        require(finished_future.get() == sync_rsp, "sync send returned wrong response");
    }
}

int main()
{
    try
    {
        run();
        std::cout << "Requestor tests passed\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Requestor tests failed: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
