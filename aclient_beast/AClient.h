#pragma once
#ifndef ASIO_NO_DEPRECATED
#define ASIO_NO_DEPRECATED  // 开发时启用。但此宏和 websocketpp 项目不兼容
#endif // !ASIO_NO_DEPRECATED

#ifndef _RESUMABLE_FUNCTIONS_SUPPORTED
#define _RESUMABLE_FUNCTIONS_SUPPORTED // 在 vc2015 update3 中使用协程
#endif // !_RESUMABLE_FUNCTIONS_SUPPORTED

#include <boost\beast.hpp>
#include <boost\beast\ssl.hpp>
#include <boost\container\detail\singleton.hpp>
#include <boost\asio.hpp>
#include <boost\asio\ssl.hpp>
#include <spdlog\spdlog.h>

namespace asio = boost::asio;
//using error_code = boost::system::error_code;  // std::error_code
namespace beast = boost::beast;
namespace http = beast::http;
using task_t = http::request<http::string_body>;

class HTTPRequest;
class HTTPResponse final
{
public:
    unsigned int get_status_code() const;
    std::string get_status_message() const;
    // 入参忽略大小写；返回小写
    std::string get_header(std::string) const;
    std::string get_etag() const;
    const std::string& get_string_body() const;
private:
    friend class HTTPRequest;
    HTTPResponse(http::response<http::string_body> res) : res_(std::move(res)) {}
    http::response<http::string_body> res_;
};

class HTTPRequest final: public std::enable_shared_from_this<HTTPRequest>
{
    friend class HTTPClient;
    HTTPRequest(asio::io_context& ioc, unsigned int id);
    // 所有的异步回调全都依赖 this 指针。但不提供拷贝构造&移动构造的类型，无法作为 vector 的元素
    HTTPRequest(const HTTPRequest&) = delete;
    HTTPRequest& operator==(const HTTPRequest&) = delete;
    HTTPRequest(HTTPRequest&&) = delete;
    HTTPRequest& operator==(HTTPRequest&&) = delete;
public:
    using Callback = std::function<void(const HTTPRequest&, const HTTPResponse&, const std::error_code&)>;
    ~HTTPRequest();
    static std::shared_ptr<HTTPRequest> create_request(asio::io_context& ioc, unsigned int id)
    {
        return std::shared_ptr<HTTPRequest>(new HTTPRequest(ioc, id));
    }

    // TODO 以下 set_xx 属性是否应该移到构造中？如何避免执行过程中的修改？

    /// <param name="port">为零时根据协议类型使用默认端口。http 80 https 443</param>
    void set_task(task_t task, bool https = true, unsigned port = 0);
    void set_callback(Callback hl);
    void execute();
    void cancel();
    //
    const std::string& get_host() const { return host_; }
    const task_t& get_task() const { return task_; }
    unsigned get_port() const { return port_; }
    unsigned get_id() const { return uuid_; }
private:
    void set_stream(bool https , unsigned port = 0);
    // 如果通信时采用了 gzip 压缩，解压缩并回写（buf 和 size）
    void handle_gzip();
    void finish(boost::system::error_code, std::string msg = "");
private:
    std::string host_;
    unsigned port_;
    task_t task_;   // 用于扩展 HTTPRequest 的功能
    beast::flat_buffer buffer_;
    http::response<http::string_body> res_;
    Callback handler_;
    unsigned int uuid_;
    //
    asio::io_context& ioc_;
    asio::ip::tcp::resolver resolver_;
    // 和 ssocket_(+ctx_) 二选一
    std::shared_ptr<beast::tcp_stream> insocket_;
    std::shared_ptr<asio::ssl::context> ctx_;    // TODO reference?
    std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> ssocket_;
};

template<typename T>
using Singleton = boost::container::dtl::singleton_default<T>;

class WebSocketRequest;
class HTTPClient
{
    friend struct boost::container::dtl::singleton_default<HTTPClient>;
    HTTPClient();
public:
    std::shared_ptr<HTTPRequest> create_request(unsigned id)
    {
        return std::shared_ptr<HTTPRequest>(new HTTPRequest(ioc_, id));
    }
    std::shared_ptr<WebSocketRequest> create_websocket(unsigned id);
    std::shared_ptr<asio::steady_timer> create_timer(std::chrono::milliseconds timeout)
    {
        return std::make_shared<asio::steady_timer>(ioc_, timeout);
    }
    // 撤销守护，等待所有任务执行完毕
    void wait();
    /// <summary>
    /// 主动关停，丢弃尚未执行的任务 NOTE 建议显式调用关闭。
    /// 因为单例静态对象的释放顺序不可控（类似其构造），当异步 IO 的
    /// 回调中使用其他静态变量（或全局变量）时，大概率上因其析构出现莫名其妙的错误
    /// </summary>
    void close();
    ~HTTPClient();
private:
    asio::io_context ioc_;
    std::thread thIoc_;
    using ioc_guard_t = asio::executor_work_guard<decltype(ioc_.get_executor())>;
    ioc_guard_t guard_;
};

namespace gzip
{
    inline bool is_compressed(const char* data, std::size_t size) { return false; };
    inline std::string decompress(const char* data, std::size_t size) { return{}; };
}
