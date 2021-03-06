#pragma once
#ifndef ASIO_NO_DEPRECATED
#define ASIO_NO_DEPRECATED  // 开发时启用。但此宏和 websocketpp 项目不兼容
#endif // !ASIO_NO_DEPRECATED

#include <fstream>
#include <asio.hpp>
#include <asio/ssl.hpp>
#include <iostream>
#include <spdlog\spdlog.h>
#include <queue>
#include <map>
#include "Utility.h"

class HTTPRequest;
class HTTPResponse final
{
public:
    unsigned int get_status_code() const;
    const std::string& get_status_message() const;
    const std::map<std::string, std::string>& get_headers() const;
    // 入参忽略大小写；返回小写
    std::string get_header(std::string) const;
    std::string get_etag() const;
    asio::streambuf& get_response_buf();
    void reset();
    // 以下写操作需要鉴权
    void set_status_code(unsigned status_code, Passkey<HTTPRequest>);
    void set_status_message(std::string status_message, Passkey<HTTPRequest>);
    void add_header(std::string name, std::string value, Passkey<HTTPRequest>);
private:
    // http status code & message
    unsigned int m_status_code;
    std::string m_status_message;
    // response headers.
    std::map<std::string, std::string> m_headers;
    //
    asio::streambuf m_response_buf;
};

class task_t final
{
public:
    task_t() = default;
    /*explicit*/ task_t(std::string page) : page_(std::move(page)) {};
    // 改用 POST 方式通信，会覆盖掉 enable_etag() 操作
    void set_data(std::string data);
    // 启用 etag，与 set_data() 操作冲突
    void enable_etag();
    // 入参应满足 key:value 格式，且不能包含 \r\n
    void add_header(const std::string& h);
    //
    bool isPostTask() const { return !data_.empty(); }
    const std::string& page() const { return page_; }
    const std::string& data() const { return data_; }
    bool need_etag() const { return etag_; }
    std::string request_msg(const std::string& host, unsigned port, const std::string& etag/* = ""*/) const;
    // POST 返回空字符串
    std::string get_url(const std::string& host, unsigned port = 80, bool https= false) const;

private:
    std::string page_;
    std::string data_;    // POST: data
    bool etag_ = false;   // true 建议启用 etag
    std::vector<std::string> headers_;
};

class HTTPRequest : public std::enable_shared_from_this<HTTPRequest>
{
    static std::atomic<unsigned long> count;
    friend class HTTPClient;
    static const unsigned DEFAULT_PORT = 80;
    static const unsigned DEFAULT_SECURITY_PORT = 443;

protected:
    HTTPRequest(asio::io_context& ioc, unsigned int id) :
        ioc_(ioc), uuid_(id), port_(DEFAULT_PORT), resolver_(ioc_), was_cancel_(false)
    {
#ifdef _DEBUG
        ++count;
        if (count % 1000 == 0)
        {
            spdlog::warn("There are {} sockets.", count);
        }
#endif // _DEBUG
    }
    asio::io_context& ioc() { return ioc_; }
private:
    // 所有的异步回调全都依赖 this 指针。但不提供拷贝构造&移动构造的类型，无法作为 vector 的元素
    HTTPRequest(const HTTPRequest&) = delete;
    HTTPRequest& operator==(const HTTPRequest&) = delete;
    HTTPRequest(HTTPRequest&&) = delete;
    HTTPRequest& operator==(HTTPRequest&&) = delete;
public:
    using task_t = ::task_t;
    // status_code == 304 时会填充 HTTPResponse::m_response_buf
    using Callback = std::function<void(const HTTPRequest&, /*const*/ HTTPResponse&, const std::error_code&)>;
    virtual ~HTTPRequest()
    {
#ifdef _DEBUG
        --count;
#endif // _DEBUG
    }
    //
    static std::shared_ptr<HTTPRequest> create_request(asio::io_context& ioc, unsigned int id)
    {
        return std::shared_ptr<HTTPRequest>(new HTTPRequest(ioc, id));
    }

    // TODO 以下 set_xx 属性是否应该移到构造中？如何避免执行过程中的修改？

    /// <param name="host">可以携带协议前缀，比如 https:// 或 http:// ，无前缀默认 http://</param>
    /// <param name="port">为零时根据协议类型使用默认端口。http 80 https 443</param>
    void set_host(std::string host, unsigned port = 0);
    void set_task(task_t task);
    void set_callback(Callback hl);
    //
    const std::string& get_host() const { return host_; }
    unsigned get_port() const { return port_; }
    const task_t& get_task() const { return task_; }
    unsigned get_id() const { return uuid_; }
    //
    virtual void execute();
    virtual void cancel();
private:
    asio::awaitable<void> handle_response(const std::string&);
    // 返回值 content_length，如果没有则返回 -1
    long read_header(std::istream& in);
    // 如果通信时采用了 gzip 压缩，解压缩并回写（buf 和 size）
    void handle_gzip();
    void finish(const std::error_code&, std::string msg = "");
private:
    std::string host_;
    unsigned port_;
    task_t task_;   // 用于扩展 HTTPRequest 的功能
    std::string request_;
    HTTPResponse response_;
    Callback handler_;
    bool was_cancel_;
    unsigned int uuid_;
    //
    asio::io_context& ioc_;
    asio::ip::tcp::resolver resolver_;
    // 和 ssocket_(+ctx_) 二选一
    std::shared_ptr<asio::ip::tcp::socket> insocket_;
    std::shared_ptr<asio::ssl::context> ctx_;    // TODO reference?
    std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket>> ssocket_;

};

class HTTPClient : public Singleton<HTTPClient>
{
    friend class Singleton<HTTPClient>;
    HTTPClient();
public:
    std::shared_ptr<HTTPRequest> create_request(unsigned id)
    {
        return std::shared_ptr<HTTPRequest>(new HTTPRequest(ioc_, id));
    }
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
