#include "AClient.h"
#include "http_errors.h"

using namespace std;

std::atomic<unsigned long> HTTPRequest::count = 0;

void default_handler(const HTTPRequest& request, HTTPResponse& response, const std::error_code& ec)
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

void HTTPRequest::set_stream(bool https, unsigned port)
{
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
        port_ = port > 0 ? port : DEFAULT_SECURITY_PORT;
    }
    else
    {
        insocket_ = std::make_shared<beast::tcp_stream>(ioc_);
        if (insocket_)
        {
            ssocket_.reset();
        }
        port_ = port > 0 ? port : DEFAULT_PORT;
    }
}

void HTTPRequest::set_task(task_t task, bool https, unsigned port)
{
    const auto ctor = task.find(http::field::host);
    if (task.cend() == ctor) {
        assert(false);
        spdlog::warn("there is no host.");
        return;
    }
    std::string host = ctor->value().to_string();
    if (host.find("http://") != string::npos ||
        host.find("https://") != string::npos)
    {
        assert(false);
        spdlog::warn("the host start with http/s.");
        return;
    }
    std::swap(host_, host);
    this->set_stream(https, port);
    task.version(10);   // only support 1.0 which has no chunked
    //task.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    swap(task, task_);
}

void HTTPRequest::execute()
{
    assert(port_ > 0);
    assert(host_.empty() == false);
    assert((ctx_ == nullptr) == (ssocket_ == nullptr));
    assert((insocket_ == nullptr) != (ssocket_ == nullptr));

    res_.clear();
    std::unique_lock<mutex> ulk(cancel_mutex_);
    if (was_cancel_)
    {
        ulk.unlock();
        finish(asio::error::operation_aborted);
        return;
    }
    using asio::ip::tcp;
    resolver_.async_resolve(tcp::v4(), host_, std::to_string(port_), [this, self = shared_from_this()](boost::system::error_code ec, tcp::resolver::results_type ep) {
        self;
        handle_resolve(ec, ep);
    });
}

void HTTPRequest::handle_resolve(boost::system::error_code ec, asio::ip::tcp::resolver::results_type endpoints)
{
    using asio::ip::tcp;
    if (ec) {
        finish(ec);
        return;
    }
    std::unique_lock<mutex> ulk(cancel_mutex_);
    if (was_cancel_) {
        ulk.unlock();
        finish(asio::error::operation_aborted);
        return;
    }
    // 二选一
    assert((nullptr == ssocket_) != (nullptr == insocket_));
    auto & stream = ssocket_ ? beast::get_lowest_layer(*ssocket_) : *insocket_;
    stream.expires_after(30s);
    stream.async_connect(endpoints, [this, self = shared_from_this()](boost::system::error_code ec, tcp::endpoint ep) {
        self;
        handle_connect(ec);
    });
}

void HTTPRequest::handle_connect(boost::system::error_code ec)
{
    if (ec) {
        finish(ec);
        return;
    }
    std::unique_lock<mutex> ulk(cancel_mutex_);
    if (was_cancel_) {
        ulk.unlock();
        finish(asio::error::operation_aborted);
        return;
    }
    // 二选一
    if (insocket_)
    {
        auto & stream = *insocket_;
        http::async_write(stream, task_, [this, self = shared_from_this()](boost::system::error_code ec, std::size_t len) {
            handle_write(ec, len);
        });
        assert(ssocket_ == nullptr);
    }
    else if (ssocket_)
    {
        ssocket_->async_handshake(asio::ssl::stream_base::client, [this, self = shared_from_this()](boost::system::error_code ec){
            handle_handshake(ec);
        });
    }
}

void HTTPRequest::handle_handshake(boost::system::error_code ec)
{
    // http 通信不应进入此函数
    assert(nullptr == insocket_);
    if (ec) {
        finish(ec);
        return;
    }
    std::unique_lock<mutex> ulk(cancel_mutex_);
    if (was_cancel_) {
        ulk.unlock();
        finish(asio::error::operation_aborted);
        return;
    }
    // 二选一
    if (ssocket_)
    {
        http::async_write(*ssocket_, task_, [this, self = shared_from_this()](boost::system::error_code ec, std::size_t len) {
            handle_write(ec, len);
        });
        assert(insocket_ == nullptr);
    }
    else if (insocket_)
    {
        assert("未知错误- http 通信不应该进入此函数" && false);
    }
}

void HTTPRequest::handle_write(boost::system::error_code ec, std::size_t len)
{
    if (ec) {
        finish(ec, "async_write");
        return;
    }
    // NOTE 有的服务端在客户端关闭写（继续读）之后，会直接关闭连接
    //socket_.shutdown(asio::socket_base::shutdown_send);
    std::unique_lock<mutex> ulk(cancel_mutex_);
    if (was_cancel_) {
        ulk.unlock();
        finish(asio::error::operation_aborted);
        return;
    }
    //if (request_.size() == len)
    {
        if (ssocket_)
        {
            boost::beast::http::async_read(*ssocket_, buffer_, res_, [this, self = shared_from_this()](boost::system::error_code ec, std::size_t len) {
                std::cout << (res_.base()) << std::endl;
                spdlog::info("{}", res_.body().length());
                std::cout << (res_.body()) << std::endl;
            });
            assert(insocket_ == nullptr);
        }
        else if (insocket_)
        {
            boost::beast::http::async_read(*insocket_, buffer_, res_, [this, self = shared_from_this()]
            (boost::system::error_code ec, std::size_t len) {
                std::cout << (res_.base()) << std::endl;
                spdlog::info("{}", res_.body().length());
                spdlog::info(res_.body());
                spdlog::info("");
                std::cout << (res_.body()) << std::endl;
                finish(ec);
            });
        }
    }
}
void HTTPRequest::handle_read_content(boost::system::error_code ec, std::size_t)
{
    if (asio::error::eof/*2*/ == ec)
    {
        finish(boost::system::error_code());
    }
#if _WIN32_WINNT>=0x0600 && SSL_R_SHORT_READ   // 细节见《short read workaround.md》
    // TODO maybe incorrect?
    else if (ec.category() == asio::error::get_ssl_category() &&
        ec.value() == ERR_PACK(ERR_LIB_SSL, 0, SSL_R_SHORT_READ)) {
        // -> not a real error, just a normal TLS shutdown
        do
        {
            boost::system::error_code err;
            assert(nullptr != ssocket_);
            auto & socket_ = ssocket_->lowest_layer();
            auto ep = socket_.remote_endpoint(err);
            auto ip = err ? "null" : ep.address().to_string();
            spdlog::warn("async_read faild:{}, {}. http{}://{}({}):{}@{}", ec.value(), ec.message(),
                ssocket_ ? "s" : "",
                host_, ip, port_, static_cast<void*>(this));
        } while (false);

        finish(boost::system::error_code());
    }
#endif
    else
    {
        finish(ec, "async_read");
    }
}

void HTTPRequest::finish(const boost::system::error_code& ec, std::string msg)
{
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
    if (ec && (ec.value() != asio::error::operation_aborted))
    {

        spdlog::error("{} faild:{}, {}. http{}://{}({}):{}@{}", msg, ec.value(), ec.message(),
            ssocket_ ? "s" : "",
            host_, ip, port_, static_cast<void*>(this));
    }
    if (!ec)
    {
        // finish
    }
    else if (asio::error::operation_aborted /*995*/ == ec)
    {
        // cancel
    }
    HTTPResponse tmp{ res_ };
    const string gzip = tmp.get_header("content-encoding");
    if (gzip.find("gzip") != std::string::npos) {
        handle_gzip();
    }
    if (handler_)
    {
        if (asio::error::eof == ec)  // maybe incorrect?
        {
            // 是否正确存在疑问，所以没有缓存
            handler_(*this, tmp, std::error_code());
        }
        else
        {
            handler_(*this, tmp, ec);
        }
    }
    else
    {
        default_handler(*this, tmp, ec);
    }
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
#if !defined(_WIN32_WINNT) || _WIN32_WINNT <= 0x502 // Windows Server 2003 or earlier.
    spdlog::warn("Not implemented '{}' in Windows Server 2003 or earlier.", __FUNCTION__);
#else
    unique_lock<mutex> ulk(cancel_mutex_);
    was_cancel_ = true;
    resolver_.cancel();
    // 二选一
    assert((nullptr == ssocket_) != (nullptr == insocket_));
    auto & socket_ = ssocket_ ? beast::get_lowest_layer(*ssocket_) : *insocket_;
    if (socket_.socket().is_open())
    {
        socket_.cancel();
    }
#endif
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
    using utility::tolower;
    header = tolower(header);
    auto tgt = res_.find(header);
    string value = (tgt != res_.cend() ? tgt->value().to_string() : "");
    return tolower(value);
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
