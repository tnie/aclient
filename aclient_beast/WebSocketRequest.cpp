#include "WebSocketRequest.h"

using namespace std;

std::shared_ptr<WebSocketRequest> HTTPClient::create_websocket(unsigned id)
{
    return std::shared_ptr<WebSocketRequest>(new WebSocketRequest(ioc_, id));
}

WebSocketRequest::~WebSocketRequest()
{
}

void WebSocketRequest::set_task(std::string host, bool https, std::string target, unsigned port)
{
    if (host.find("http://") != string::npos ||
        host.find("https://") != string::npos ||
        host.find("ws://") != string::npos ||
        host.find("wss://") != string::npos)
    {
        assert(false);
        spdlog::warn("the host can't start with http/ws/https/wss.");
        return;
    }
    std::swap(host_, host);
    std::swap(target_, target);
    if (https)
    {
        ctx_ = std::make_shared<asio::ssl::context>(asio::ssl::context::sslv23_client);
        if (ctx_)
        {
            ssocket_ = std::make_shared<websocket::stream<beast::ssl_stream<beast::tcp_stream>>>(ioc_, *ctx_);
            if (ssocket_)
            {
                ws_.reset();
            }
        }
        port_ = port > 0 ? port : DEFAULT_SECURITY_PORT;
    }
    else
    {
        ws_ = std::make_shared<websocket::stream<beast::tcp_stream>>(ioc_);
        if (ws_)
        {
            ssocket_.reset();
        }
        port_ = port > 0 ? port : DEFAULT_PORT;
    }
}

void WebSocketRequest::send(const std::string & msg)
{
    asio::co_spawn(ioc_, [this, self = shared_from_this(), msg]()->asio::awaitable<void> {
        try
        {
            // 关注 msg 的生存期
            std::size_t bytes_transferred;
            if (ws_) {
                bytes_transferred = co_await ws_->async_write(asio::buffer(msg), asio::use_awaitable);
                assert(ssocket_);
            }
            else if (ssocket_) {
                bytes_transferred = co_await ssocket_->async_write(asio::buffer(msg), asio::use_awaitable);
            }
            if (bytes_transferred != msg.length()) {
                spdlog::error("write msg failed. {}", msg);
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
    }, asio::detached);
}

void WebSocketRequest::close()
{
    asio::post(ioc_, [this, self = shared_from_this()]() {
        auto handler = [self = shared_from_this()](beast::error_code ec) {
            if (ec == boost::asio::error::eof) {
                // 技术错误。但业务上无关紧要
                spdlog::warn("{} {}:{}", ec.message(), __FILE__, __LINE__);
            }
            else if (ec) {
                spdlog::error("close websocket failed. {} {}:{}",
                    ec.message(), __FILE__, __LINE__);
            }
        };
        // Close the WebSocket connection
        if (ws_) {
            ws_->async_close(websocket::close_code::normal, handler);
        }
        else if (ssocket_) {
            ssocket_->async_close(websocket::close_code::normal, handler);
        }
    });
}

void WebSocketRequest::execute()
{
    assert(port_ > 0);
    assert(host_.empty() == false);
    assert((ctx_ == nullptr) == (ssocket_ == nullptr));
    assert((ws_ == nullptr) != (ssocket_ == nullptr));

    using asio::ip::tcp;
    asio::co_spawn(ioc_, [this, self = shared_from_this()]()->asio::awaitable<void> {
        try
        {
            asio::ip::tcp::resolver::results_type endpoints =
                co_await resolver_.async_resolve(tcp::v4(), host_, std::to_string(port_), asio::use_awaitable);
            spdlog::debug("async_resolve() done.");
            // 二选一
            assert((nullptr == ssocket_) != (nullptr == ws_));
            auto & stream = ssocket_ ? beast::get_lowest_layer(*ssocket_) : beast::get_lowest_layer(*ws_);
            stream.expires_after(30s);
            asio::ip::tcp::endpoint endpoint =
                co_await stream.async_connect(endpoints, asio::use_awaitable);
            spdlog::debug("async_connect() done.");
            if (ssocket_) {
                // http 通信不应进入此函数
                co_await ssocket_->next_layer().async_handshake(asio::ssl::stream_base::client, asio::use_awaitable);
                spdlog::debug("ssl async_handshake() done.");
            }
            // Turn off the timeout on the tcp_stream, because
            // the websocket stream has its own timeout system.
            stream.expires_never();
            // Perform the websocket handshake
            const string host = host_ + ":" + std::to_string(port_);
            if (ws_)
            {
                // Set suggested timeout settings for the websocket
                ws_->set_option(
                    beast::websocket::stream_base::timeout::suggested(
                        beast::role_type::client));
                co_await ws_->async_handshake(host, target_, asio::use_awaitable);
                spdlog::debug("websocket async_handshake() done.");
                //std::size_t len = co_await ws_->async_write(asio::buffer(""), asio::use_awaitable);
                do
                {
                    std::size_t len = co_await ws_->async_read(buffer_, asio::use_awaitable);
                    // The make_printable() function helps print a ConstBufferSequence
                    std::cout << beast::make_printable(buffer_.data()) << std::endl;
                    buffer_.clear();
                } while (true);
            }
            else if (ssocket_)
            {
                // Set suggested timeout settings for the websocket
                ssocket_->set_option(
                    beast::websocket::stream_base::timeout::suggested(
                        beast::role_type::client));
                co_await ssocket_->async_handshake(host, "/", asio::use_awaitable);
                //std::size_t len = co_await ssocket_->async_write(asio::buffer(""), asio::use_awaitable);
                do
                {
                    std::size_t len = co_await ssocket_->async_read(buffer_, asio::use_awaitable);
                    // The make_printable() function helps print a ConstBufferSequence
                    std::cout << beast::make_printable(buffer_.data()) << std::endl;
                    buffer_.clear();
                } while (true);
                assert(ws_ == nullptr);
            }
        }
        catch (const boost::system::system_error& e)
        {
            const auto& ec = e.code();
            if (ec == boost::beast::websocket::error::closed ||
                ec == boost::asio::error::operation_aborted ||
                ec == boost::asio::error::eof
                )
            {
                spdlog::warn("{} {}:{}", e.what(), __FILE__, __LINE__);
            }
            else {
                spdlog::error("{} {}:{}", e.what(), __FILE__, __LINE__);
            }
        }
        catch (const std::exception& e)
        {
            spdlog::error("{} {}:{}", e.what(), __FILE__, __LINE__);
        }
    }, asio::detached);
}

