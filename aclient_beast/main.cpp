#include "AClient.h"
#include <curl\curl.h>
int main()
{
    constexpr unsigned tag = 1;
    auto request = HTTPClient::getInstance().create_request(tag);
    task_t task;
    task.method(http::verb::get);
    task.target("/");
    task.set(http::field::host, "127.0.0.1");
    request->set_task(task, false, 55555);
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
    HTTPClient::getInstance().wait();
}