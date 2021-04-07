#include "AClient.h"

int main()
{
    constexpr unsigned tag = 1;
    auto request = HTTPClient::getInstance().create_request(tag);
    task_t task;
    task.method(http::verb::get);
    task.target("/");
    task.set(http::field::host, "www.baidu.com");
    request->set_task(task);
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
        const std::string gzip = hr.get_header("content-encoding");
        std::istream response_stream(&hr.get_response_buf());
        std::istreambuf_iterator<char> eos;
        auto msg = std::string(std::istreambuf_iterator<char>(response_stream), eos);
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