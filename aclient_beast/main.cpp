#include "AClient.h"
#include "WebSocketRequest.h"
#include <curl\curl.h>

using namespace std;
void test_http()
{
    constexpr unsigned tag = 1;
    auto request = Singleton<HTTPClient>::instance().create_request(tag);
    task_t task;
    task.method(http::verb::get);
    task.target("/beast_server.cpp");
    task.set(http::field::host, "127.0.0.1");
    request->set_task(task, false);
    request->set_callback([](const HTTPRequest& dummy, HTTPResponse& hr, const std::error_code& ec) {
        if (ec || hr.get_status_code() != 200)
        {
            if (ec)
            {
                spdlog::warn("http[s] failed:[{}:{}], {}", ec.category().name(), ec.value(), ec.message());
            }
            if (hr.get_status_code() != 200)
            {
                spdlog::warn("http[s] failed: status_code[{}]", hr.get_status_code());
            }
            return;
        }
        spdlog::info("date is: {}", hr.get_header("date"));
        auto tt = curl_getdate(hr.get_header("date").c_str(), nullptr);
        if (tt < 0) {
            spdlog::warn("curl_getdate failed. {}", hr.get_header("date"));
            return;
        }
        else {
            spdlog::info("date is: {}", tt);
            time_t rawtime = time(nullptr);
            spdlog::info("loca is: {}", rawtime);
        }
        const std::string gzip = hr.get_header("content-encoding");
        const auto& msg = hr.get_string_body();
        if (gzip.find("gzip") != std::string::npos)
        {
            if (gzip::is_compressed(msg.c_str(), msg.size()))
            {
                auto content = gzip::decompress(msg.c_str(), msg.size());
            }
        }
        else {
            spdlog::info(msg);
        }
    });
    request->execute();
}

auto test_ws()
{
    constexpr unsigned tag = 1;
    auto request = Singleton<HTTPClient>::instance().create_websocket(tag);
    request->set_task("devyun.ydtg.com.cn", false);
    request->execute();
    return std::async(std::launch::async, [=]() {
        std::this_thread::sleep_for(3s);
        request->close();
    });
}
int main()
{
    SetConsoleOutputCP(CP_UTF8);
    test_ws();
    Singleton<HTTPClient>::instance().wait();
}