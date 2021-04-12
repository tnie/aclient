#include "AClient.h"
#include "http_errors.h"

using namespace std;

std::atomic<unsigned long> HTTPRequest::count = 0;

void default_handler(const HTTPRequest& request, HTTPResponse& response, const std::error_code& ec)
{
    if (ec.value() == 0)
    {
        std::cout << "Request #" << request.get_id() << " has completed. Response: "
            << &response.get_response_buf();
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

std::string task_t::request_msg(const std::string& host, unsigned port, const std::string& etag /*= ""*/) const
{
    assert(host.empty() == false);
    assert(port > 0);
    assert(page_.empty() == false);
    std::ostringstream oss;
    if (isPostTask() == false)
    {
        oss << "GET " << page_ << " HTTP/1.0\r\n";
        oss << "Host: " << host << ":" << port << "\r\n";
        if (etag.size() && need_etag())
        {
            //oss << "If-Modified-Since: " << lm << "\n";
            oss << "If-None-Match: " << etag << "\r\n";
        }
        //oss << "Connection: keep-alive\r\n";
        //oss << "Connection: close\r\n";
        //oss << "Accept-Encoding: identity\r\n";    // 暂时禁止压缩算法
        oss << "Accept-Encoding: gzip\r\n";         // 启用 gzip 压缩
        for (const auto& header : headers_)
        {
            oss << header << "\r\n";
        }
        oss << "\r\n";
    }
    else
    {
        oss << "POST " << page_ << " HTTP/1.0\r\n";
        oss << "Host: " << host << ":" << port << "\r\n";
        oss << "Accept: */*\r\n";
        oss << "Content-Length: " << data_.length() << "\r\n";
        //oss << "Content-Type: application/x-www-form-urlencoded\r\n";
        for (const auto& header : headers_)
        {
            oss << header << "\r\n";
        }
        oss << "Connection: close\r\n\r\n";
        oss << data_;
    }
    return oss.str();
}

void task_t::set_data(std::string data)
{
    if (data.empty())
    {
        // isPostTask() 存在缺陷，借此修正
        // TODO
        data.append("POST");
    }
    data_.swap(data);
    etag_ = false;
}

void task_t::add_header(const std::string& h)
{
    assert(h.find(":") != std::string::npos);
    assert(h.find("\r") == std::string::npos);
    assert(h.find("\n") == std::string::npos);
    headers_.push_back(h);
}

std::string task_t::get_url(const std::string& host, unsigned port /*= 80*/, bool https/*= false*/) const
{
    if (isPostTask())
    {
        return "";
    }
    return fmt::format("http{}://{}:{}/{}", (https ? "s": ""), host, port, page_);
}

void task_t::enable_etag()
{
    etag_ = !isPostTask();
}

void HTTPRequest::set_host(std::string host, unsigned port)
{
    if (host.find("https://") != std::string::npos)
    {
        const static size_t protocolength = std::string("https://").length();
        ctx_ = std::make_shared<asio::ssl::context>(asio::ssl::context::sslv23_client);
        if (ctx_)
        {
            ssocket_ = std::make_shared<asio::ssl::stream<asio::ip::tcp::socket>>(ioc_, *ctx_);
            if (ssocket_)
            {
                insocket_.reset();
            }
        }
        host_ = host.substr(protocolength);
        port_ = port > 0 ? port : DEFAULT_SECURITY_PORT;

    }
    else
    {
        insocket_ = std::make_shared<asio::ip::tcp::socket>(ioc_);
        if (insocket_)
        {
            ssocket_.reset();
        }
        if (host.find("http://") != std::string::npos)
        {
            const static size_t protocolength = std::string("http://").length();
            host_ = host.substr(protocolength);
        }
        else
        {
            host_.swap(host);
        }
        port_ = port > 0 ? port : DEFAULT_PORT;
    }

}

void HTTPRequest::set_task(task_t task)
{
    swap(task, task_);
    request_.clear();
}

void HTTPRequest::execute()
{
    assert(port_ > 0);
    assert(host_.empty() == false);
    if (request_.empty())
    {
        string etag;
        if (task_.need_etag())
        {
            auto url = task_.get_url(host_, port_, (insocket_ == nullptr));
            if (!url.empty())
            {
                // 从本地缓存等位置读出
                //etag = AClientOper::getInstance().get_etag(url);
            }
        }
        request_ = task_.request_msg(host_, port_, etag);
    }
    assert(request_.empty() == false);
    assert((ctx_ == nullptr) == (ssocket_ == nullptr));
    assert((insocket_ == nullptr) != (ssocket_ == nullptr));

    response_.reset();
    using asio::ip::tcp;
    asio::co_spawn(ioc_, [this, self = shared_from_this()]()->asio::awaitable<void> {
        try
        {
            auto endpoints = co_await resolver_.async_resolve(tcp::v4(), host_, std::to_string(port_), asio::use_awaitable);
            // 二选一
            assert((nullptr == ssocket_) != (nullptr == insocket_));
            auto & socket_ = ssocket_ ? ssocket_->lowest_layer() : *insocket_;
            co_await async_connect(socket_, endpoints, asio::use_awaitable);
            assert(false == request_.empty());
            if (ssocket_)
            {
                co_await ssocket_->async_handshake(asio::ssl::stream_base::client, asio::use_awaitable);
            }
            assert((nullptr == ssocket_) != (nullptr == insocket_));
            auto bytes = insocket_ ?    // 二选一
                co_await asio::async_write(*insocket_, asio::buffer(request_), asio::use_awaitable) :
                co_await asio::async_write(*ssocket_, asio::buffer(request_), asio::use_awaitable);
            // NOTE 有的服务端在客户端关闭写（继续读）之后，会直接关闭连接
            //socket_.shutdown(asio::socket_base::shutdown_send);
            assert(request_.size() == bytes);
            size_t len = insocket_ ?
                co_await asio::async_read_until(*insocket_, response_.get_response_buf(), "\r\n", asio::use_awaitable) :
                co_await asio::async_read_until(*ssocket_, response_.get_response_buf(), "\r\n", asio::use_awaitable);
            co_await handle_response(request_);
        }
        catch (const std::system_error& e)
        {
            asio::error_code ec = was_cancel_ ? asio::error::operation_aborted : e.code();
            finish(ec, e.what());
            co_return;
        }
        catch (const std::exception& e)
        {
            spdlog::error("{} {}:{}", e.what(), __FILE__, __LINE__);
            co_return;
        }
    }, asio::detached);
}

asio::awaitable<void> HTTPRequest::handle_response(const std::string& request)
{
    // Check that response is OK.
    std::istream response_stream(&response_.get_response_buf());
    unsigned int status_code = 0;
    try
    {
        std::string http_version;
        response_stream >> http_version;
        response_stream >> status_code;
        response_.set_status_code(status_code, Passkey<HTTPRequest>());
        if (!response_stream || http_version.substr(0, 5) != "HTTP/")
        {
            throw std::system_error(http_errors::invalid_response);
        }
    }
    catch (const std::exception& e)
    {
        spdlog::error("Invalid response when request {}. {} {}:{}", task_.page(),
            e.what(), __FILE__, __LINE__);
        finish(http_errors::invalid_response);
        co_return;
    }
    if (status_code == 304)
    {
        spdlog::debug("Not modified. {}", task_.page());
        // 如果要复用 socket，已接收的缓存以及 flying 的数据都需要处理
    }
    else if (status_code != 200)
    {
        spdlog::warn("Response returned with status code {}. {}", status_code, task_.page());
    }
    std::string status_message;
    std::getline(response_stream, status_message, '\r');
    // Remove symbol '\n' from the buffer
    response_stream.get();
    response_.set_status_message(std::move(status_message), Passkey<HTTPRequest>());
    auto token = asio::use_awaitable;

    try
    {
        // Read the response headers, which are terminated by a blank line.
        assert((nullptr == ssocket_) != (nullptr == insocket_));
        auto len = ssocket_ ?    // 二选一
            co_await asio::async_read_until(*ssocket_, response_.get_response_buf(), "\r\n\r\n", token) :
            co_await asio::async_read_until(*insocket_, response_.get_response_buf(), "\r\n\r\n", token);
        // Process the response headers.
        std::istream response_stream(&response_.get_response_buf());
        auto content_length = read_header(response_stream);
        const std::string conn("keep-alive");
        if ((content_length < 0) && (request.find(conn) != std::string::npos))
        {
            // 与 keep-alive 冲突：如果启用了 keep-alive 属性，会死等，巨慢
            spdlog::error("'{}' request didn't see response's 'Content-Length'. {}", task_.page());
        }
        // Start reading remaining data .
        auto size = response_.get_response_buf().size();
        const auto remain = (content_length >= size) ? (content_length - size) : std::numeric_limits<unsigned long>::max();
        const auto step = (content_length >= size) ? std::min(remain, 1024ul) : 1024ul;
        asio::error_code ec;
        //auto len = co_await asio::async_read(*ssocket_, response_.m_response_buf, asio::transfer_at_least(content_length - size),
        //    asio::redirect_error(token, ec));
        for (size_t len = 0; !ec && len < remain; )
        {
            auto buf = response_.get_response_buf().prepare(step);
            assert((nullptr == ssocket_) != (nullptr == insocket_));
            auto bytes = ssocket_ ?    // 二选一
                co_await ssocket_->async_read_some(asio::buffer(buf), asio::redirect_error(token, ec)) :
                co_await insocket_->async_read_some(asio::buffer(buf), asio::redirect_error(token, ec));
            response_.get_response_buf().commit(bytes);
            len += bytes;
        }
        if (asio::error::eof/*2*/ != ec)
            throw std::system_error(ec);
        finish(asio::error_code());
    }
    catch (const std::system_error& e)
    {
        asio::error_code ec = was_cancel_ ? asio::error::operation_aborted : e.code();
        finish(ec, e.what());
        co_return;
    }
    catch (const std::exception& e)
    {
        spdlog::error("{} {}:{}", e.what(), __FILE__, __LINE__);
        co_return;
    }
}

long HTTPRequest::read_header(std::istream& response_stream)
{
    std::string header;

    long  content_length = -1;
    while (std::getline(response_stream, header, '\r'))
    {
        // Remove symbol '\n' from the buffer
        response_stream.get();
        if (header.empty())
        {
            break;  // 空行
        }
        size_t pz = header.find(":");
        string key1 = header.substr(0, pz);
        response_.add_header(std::move(key1), header.substr(pz + 1), Passkey<HTTPRequest>());
    }
    // 针对个别属性做特殊处理
    string ssize = response_.get_header("Content-Length");
    if (!ssize.empty())
    {
        content_length = std::stol(ssize);
    }
    if (200 == response_.get_status_code())
    {
        spdlog::info("Content-length of {}'s response is {}.", task_.page(), content_length);
    }
    if (response_.get_header("transfer-encoding").empty() == false)
    {
        // 不能处理分包
        assert(header.find("chunked") != std::string::npos);
    }
    return content_length;
}

void HTTPRequest::finish(const std::error_code& ec, std::string msg)
{
    std::string ip;
    if (ip.empty())
    {
        asio::error_code err;
        // 二选一
        assert((nullptr == ssocket_) != (nullptr == insocket_));
        auto & socket_ = ssocket_ ? ssocket_->lowest_layer() : *insocket_;
        auto ep = socket_.remote_endpoint(err);
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
    handle_gzip();
    if (handler_)
    {
        if (asio::error::eof == ec)  // maybe incorrect?
        {
            // 是否正确存在疑问，所以没有缓存
            handler_(*this, response_, std::error_code());
        }
        else
        {
            if (!ec)
            {
                auto status_code = response_.get_status_code();
                const string url = task_.get_url(host_, port_, ssocket_ != nullptr);
                auto &buf = response_.get_response_buf();
                const auto size = buf.size();
                if (304 == status_code)
                {
                    assert(size == 0);
                    std::shared_ptr<char> ptr;
                    size_t length = 0;
                    //从本地缓存等位置重新读出
                    //std::tie(ptr, length) = AClientOper::getInstance().get(url);
                    if (length)
                    {
                        auto s2 = buf.sputn(ptr.get(), length);
                        assert(s2 == length);
                    }
                }
                else if (200 == status_code && task_.need_etag())
                {
                    const string etag = response_.get_etag();
                    auto   buffer = std::shared_ptr<char>(new char[size], [](char* p) {
                        delete[] p;
                    });
                    auto s2 = buf.sgetn(buffer.get(), size);
                    spdlog::info("{} (with {} @{}): {}/{} -> buffer", task_.page(), etag, ip, s2, size);
                    //更新到本地缓存等位置
                    //AClientOper::getInstance().update(url, etag, buffer, size);
                    // response_ has nothing. rewrite
                    assert(buf.size() == 0);
                    buf.sputn(buffer.get(), size);
                }
            }

            handler_(*this, response_, ec);
        }
    }
    else
    {
        default_handler(*this, response_, ec);
    }
}

void HTTPRequest::handle_gzip()
{
    const string gzip = response_.get_header("content-encoding");
    if (gzip.find("gzip") != std::string::npos)
    {
        std::istream response_stream(&response_.get_response_buf());
        std::istreambuf_iterator<char> eos;
        const string msg = string(std::istreambuf_iterator<char>(response_stream), eos);
        spdlog::info("Response of {} has 'gzip', and size is {}.", task_.page(), msg.size());
        if (gzip::is_compressed(msg.c_str(), msg.size()))
        {
            string content = gzip::decompress(msg.c_str(), msg.size());
            // 写回 response_
            if (size_t size = content.size())
            {
                auto s2 = response_.get_response_buf().sputn(content.data(), size);
                assert(s2 == size);
                response_.add_header("content-length", std::to_string(s2), Passkey<HTTPRequest>()); // 更新大小
                response_.add_header("content-encoding", "", Passkey<HTTPRequest>());   // 抹掉 ‘gzip’
                spdlog::info("{} override buf & content-length: {} -> buffer.", task_.page(), s2);
            }
        }
        else
        {
            const string url = task_.get_url(host_, port_, ssocket_ != nullptr);
            spdlog::error("gzip decompress failed. {}", url);
        }
    }
}

void HTTPRequest::cancel()
{
#if !defined(_WIN32_WINNT) || _WIN32_WINNT <= 0x502 // Windows Server 2003 or earlier.
    spdlog::warn("Not implemented '{}' in Windows Server 2003 or earlier.", __FUNCTION__);
#else
    std::weak_ptr<HTTPRequest> wptr = shared_from_this();
    asio::post(ioc_, [this, wptr]() {
        if (auto ptr = wptr.lock())
        {
            was_cancel_ = true;
            resolver_.cancel();
            // 二选一
            assert((nullptr == ssocket_) != (nullptr == insocket_));
            auto & socket_ = ssocket_ ? ssocket_->lowest_layer() : *insocket_;
            if (socket_.is_open())
            {
                socket_.cancel();
                socket_.close();
            }
        }
    });
#endif
}

void HTTPRequest::set_callback(Callback hl)
{
    handler_ = hl;
}

void handle_error(asio::error_code ec, const std::string& msg)
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
    return m_status_code;
}

const std::string& HTTPResponse::get_status_message() const
{
    return m_status_message;
}

const std::map<std::string, std::string>& HTTPResponse::get_headers() const
{
    return m_headers;
}

asio::streambuf& HTTPResponse::get_response_buf()
{
    return m_response_buf;
}

void HTTPResponse::set_status_code(unsigned status_code, Passkey<HTTPRequest>)
{
    m_status_code = status_code;
}

void HTTPResponse::set_status_message(std::string status_message, Passkey<HTTPRequest>)
{
    std::swap(m_status_message, status_message);
}

void HTTPResponse::add_header(std::string name, std::string value, Passkey<HTTPRequest>)
{
    using utility::tolower;
    name = tolower(name);
    value = tolower(value);
    if (name.find("transfer-encoding") != std::string::npos)
    {
        assert(value.find("chunked") == std::string::npos);
        if (value.find("chunked") != std::string::npos)
        {
            spdlog::error("Can't handle #chunked# temporarily: chunked");
            throw std::runtime_error("unexpected chunked");
        }
    }
    else if (name.find("etag") != std::string::npos)
    {
        const string WEAK_ETAG("w/");
        size_t pz = value.find(WEAK_ETAG);
        if (pz != std::string::npos)
        {
            //spdlog::info("weak etag: {}", value);
            value.erase(pz, WEAK_ETAG.size());
        }
    }
    m_headers[name] = value;
}

void HTTPResponse::reset()
{
    m_status_code = 0;
    m_status_message.clear();
    m_headers.clear();
    if (auto remain = m_response_buf.size())
    {
        spdlog::warn("Remove the remaining characters from the input sequence.");
        m_response_buf.consume(remain);
    }
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
    auto tgt = m_headers.find(header);
    string value = (tgt != m_headers.cend() ? tgt->second : "");
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
