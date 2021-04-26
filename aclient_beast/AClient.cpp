#include "AClient.h"
#include <boost\algorithm\string\case_conv.hpp>
#include <iostream>

using namespace std;

namespace {
    // 追踪 pending request，监控 socket 占用
    std::atomic<unsigned long> count_of_request = 0;
    constexpr unsigned DEFAULT_PORT = 80;
    constexpr unsigned DEFAULT_SECURITY_PORT = 443;
}

void default_handler(const HTTPRequest& request, const HTTPResponse& response, const std::error_code& ec)
{
    if (ec.value() == 0)
    {
        std::cout << "Request #" << request.get_id() << " has completed. Response: "
            << response.get_string_body();
    }
    else if (ec.value() == asio::error::operation_aborted)
    {
        std::cout << "Request #" << request.get_id() << " has been cancelled by the user." << std::endl;
    }
    else
    {

        std::cout << "Request #" << request.get_id() << " failed! Error code = "
            << ec.value() << ". Error message = " << ec.message()
            << std::endl;
    }
    return;
}

HTTPRequest::HTTPRequest(asio::io_context & ioc, unsigned int id) :
    ioc_(ioc), uuid_(id), resolver_(ioc_)
{
    ++count_of_request;
    if (count_of_request % 100 == 0) {
        spdlog::warn("There are {} sockets.", count_of_request);
    }
}

HTTPRequest::~HTTPRequest()
{
    --count_of_request;
}

void HTTPRequest::set_task(task_t task)
{
    //task.version(10);   // only support 1.0 which has no chunked
    task.version(11);
    //task.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    swap(task, task_);
}

void HTTPRequest::execute(const std::string& host, bool https , unsigned port)
{
    if (host.find("http://") != string::npos ||
        host.find("https://") != string::npos)
    {
        assert(false);
        spdlog::warn("the host can't start with http/s.");
        return;
    }
    if (host.empty())
    {
        assert(false);
        spdlog::warn("the host is empty.");
        return;
    }
    if (https)
    {
        ctx_ = std::make_shared<asio::ssl::context>(asio::ssl::context::sslv23_client);
        if (ctx_)
        {
            ssocket_ = std::make_shared<beast::ssl_stream<beast::tcp_stream>>(ioc_, *ctx_);
            if (ssocket_)
            {
                insocket_.reset();
            }
        }
        port = port > 0 ? port : DEFAULT_SECURITY_PORT;
    }
    else
    {
        insocket_ = std::make_shared<beast::tcp_stream>(ioc_);
        if (insocket_)
        {
            ssocket_.reset();
        }
        port = port > 0 ? port : DEFAULT_PORT;
    }
    assert((ctx_ == nullptr) == (ssocket_ == nullptr));
    assert((insocket_ == nullptr) != (ssocket_ == nullptr));
    spdlog::info("-> http{}://{}:{} @{}", (ssocket_ ? "s" : ""), host, port, static_cast<void*>(this));
    task_.set(http::field::host, host);
    res_.clear();
    using asio::ip::tcp;
    asio::co_spawn(ioc_, [this, self = shared_from_this(), host, port]() ->asio::awaitable<void> {
        try
        {
            asio::ip::tcp::resolver::results_type endpoints =
                co_await resolver_.async_resolve(tcp::v4(), host, std::to_string(port), asio::use_awaitable);
            // 二选一
            assert((nullptr == ssocket_) != (nullptr == insocket_));
            auto & stream = ssocket_ ? beast::get_lowest_layer(*ssocket_) : *insocket_;
            stream.expires_after(30s);
            co_await stream.async_connect(endpoints, asio::use_awaitable);
            if (ssocket_) {
                // http 通信不应进入此函数
                co_await ssocket_->async_handshake(asio::ssl::stream_base::client, asio::use_awaitable);
            }
            std::size_t len = co_await http::async_write(stream, task_, asio::use_awaitable);
            // NOTE 有的服务端在客户端关闭写（继续读）之后，会直接关闭连接
            //socket_.shutdown(asio::socket_base::shutdown_send);
            if (ssocket_)
            {
                std::size_t len = co_await boost::beast::http::async_read(*ssocket_, buffer_, res_, asio::use_awaitable);
                assert(insocket_ == nullptr);
            }
            else if (insocket_)
            {
                std::size_t len = co_await boost::beast::http::async_read(*insocket_, buffer_, res_, asio::use_awaitable);
            }
            finish({});
        }
        catch (const boost::system::system_error& e)
        {
            finish(e.code());
        }
        catch (const std::exception& e)
        {
            spdlog::error("{} {}:{}", e.what(), __FILE__, __LINE__);
        }
    }, asio::detached);
}

void HTTPRequest::finish(boost::system::error_code ec)
{
    auto get_ip = [this]() {
        std::string ip;
        if (ip.empty())
        {
            boost::system::error_code err;
            // 二选一
            assert((nullptr == ssocket_) != (nullptr == insocket_));
            auto & socket_ = ssocket_ ? beast::get_lowest_layer(*ssocket_) : *insocket_;
            auto ep = socket_.socket().remote_endpoint(err);
            ip = err ? "null" : ep.address().to_string();
        }
        return ip;
    };

    if (asio::error::eof/*2*/ == ec ||
        asio::ssl::error::stream_truncated == ec ||
        // -> not a real error, just a normal TLS shutdown
        asio::error::operation_aborted /*995*/ == ec
        // cancel
        )
    {
        asio::ssl::error::stream_truncated;
        spdlog::warn("http/https faild:{}, {}. @{}", ec.value(), ec.message(), static_cast<void*>(this));
        ec.clear();
    }
    else if (ec && (ec.value() != asio::error::operation_aborted))
    {
        spdlog::error("http/https faild:{}, {}. @{}", ec.value(), ec.message(), static_cast<void*>(this));
    }

    HTTPResponse tmp{ std::move(res_) };
    const string gzip = tmp.get_header("content-encoding");
    if (gzip.find("gzip") != std::string::npos) {
        handle_gzip();
    }
    if (!handler_) {
        handler_ = default_handler;
    }
    handler_ (*this, tmp, ec);
}

void HTTPRequest::handle_gzip()
{
    assert(false);
    //const string gzip = response_.get_header("content-encoding");
    //if (gzip.find("gzip") != std::string::npos)
    //{
    //    std::istream response_stream(&response_.get_response_buf());
    //    std::istreambuf_iterator<char> eos;
    //    const string msg = string(std::istreambuf_iterator<char>(response_stream), eos);
    //    spdlog::info("Response of {} has 'gzip', and size is {}.", task_.base().target().data(), msg.size());
    //    if (gzip::is_compressed(msg.c_str(), msg.size()))
    //    {
    //        string content = gzip::decompress(msg.c_str(), msg.size());
    //        // 写回 response_
    //        if (size_t size = content.size())
    //        {
    //            auto s2 = response_.get_response_buf().sputn(content.data(), size);
    //            assert(s2 == size);
    //            //response_.add_header("content-length", std::to_string(s2), Passkey<HTTPRequest>()); // 更新大小
    //            //response_.add_header("content-encoding", "", Passkey<HTTPRequest>());   // 抹掉 ‘gzip’
    //            spdlog::info("{} override buf & content-length: {} -> buffer.", task_.base().target().data(), s2);
    //        }
    //    }
    //    else
    //    {
    //        const string url;// = task_.get_url(host_, port_, ssocket_ != nullptr);
    //        spdlog::error("gzip decompress failed. {}", url);
    //    }
    //}
}

void HTTPRequest::cancel()
{
    std::weak_ptr<HTTPRequest> wptr = shared_from_this();
    asio::post(ioc_, [this, wptr]() {
        try
        {
            if (auto ptr = wptr.lock())
            {
                resolver_.cancel();
                // 二选一
                assert((nullptr == ssocket_) != (nullptr == insocket_));
                auto & stream = ssocket_ ? beast::get_lowest_layer(*ssocket_) : *insocket_;
                auto & socket = stream.socket();
                if (socket.is_open())
                {
    //https://think-async.com/Asio/asio-1.18.1/doc/asio/reference/basic_stream_socket/cancel/overload1.html
                    socket.cancel();
                    socket.close();
                }
            }
        }
        catch (const boost::system::system_error e)
        {
            spdlog::warn("{} {}:{}", e.what(), __FILE__, __LINE__);
        }
        catch (const std::exception& e)
        {
            spdlog::error("{} {}:{}", e.what(), __FILE__, __LINE__);
        }
    });
}

void HTTPRequest::set_callback(Callback hl)
{
    handler_ = hl;
}

void handle_error(boost::system::error_code ec, const std::string& msg)
{
    if ((asio::error::bad_descriptor/*10009*/ == ec) ||
        (asio::error::connection_aborted/*10053*/ == ec) ||
        (asio::error::connection_reset/*10054*/ == ec))
    {
        spdlog::info("{}:{}", ec.value(), ec.message());
    }
}

unsigned int HTTPResponse::get_status_code() const
{
    return res_.result_int();
}

std::string HTTPResponse::get_status_message() const
{
    return res_.reason().to_string();
}

const std::string& HTTPResponse::get_string_body() const
{
    return res_.body();
}

std::string HTTPResponse::get_etag() const
{
    const string etag = get_header("ETag"/*"Last-Modified"*/);
    return etag;
}

std::string HTTPResponse::get_header(std::string header) const
{
    auto tgt = res_.find(header);
    string value = (tgt != res_.cend() ? tgt->value().to_string() : "");
    boost::to_lower(value);
    return value;
}

HTTPClient::HTTPClient() : /*ioc_(1),*/ guard_(ioc_.get_executor())
{
    std::thread tmp([this]() {
        auto num = ioc_.run();
        spdlog::info("{} handlers were executed", num);
        });
    thIoc_.swap(tmp);
}

void HTTPClient::wait()
{
    guard_.reset();
    if (thIoc_.joinable())
    {
        thIoc_.join();
    }
}

void HTTPClient::close()
{
    guard_.reset();
    ioc_.stop();
    if (thIoc_.joinable())
    {
        thIoc_.join();
    }
}

HTTPClient::~HTTPClient()
{
    close();
}
