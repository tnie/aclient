#include <spdlog\spdlog.h>
#define ASIO_HAS_CO_AWAIT
#include <asio.hpp>
using namespace asio;

class Conn : public std::enable_shared_from_this<Conn>
{
    asio::ip::tcp::socket socket_;
    char data_[1024];
public:
    Conn(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}
    void read()
    {
        try
        {
            //如果非 public 继承，执行时会抛出 bad_weak_ptr 异常。
            //TODO 为什么不是编译失败呢？
            auto self = shared_from_this();
            socket_.async_read_some(asio::buffer(data_), [this, self](asio::error_code ec, size_t n) {
                if (!ec) {
                    spdlog::info("read {} bytes.", n);
                    this->write(n);
                }
                else {
                    spdlog::warn(ec.message());
                }
            });
        }
        catch (const std::exception& e)
        {
            // 比如断开连接
            spdlog::warn(e.what());
        }
    }
    void write(size_t n)
    {
        try
        {
            auto self = shared_from_this();
            asio::async_write(socket_, asio::buffer(data_, n), [this, self](asio::error_code ec, size_t m) {
                if (!ec) {
                    spdlog::info("write {} bytes.", m);
                    this->read();
                }
                else {
                    spdlog::warn(ec.message());
                }
            });
        }
        catch (const std::exception& e)
        {
            // 比如断开连接
            spdlog::warn(e.what());
        }
    }
};

class listener
{
    asio::ip::tcp::acceptor acceptor_;
public:
    listener(asio::io_context& ioc) :
        acceptor_(ioc, { asio::ip::tcp::v4(), 55555 })
    {

    }
    void listen()
    {
        spdlog::info("waiting for someone connect ...");
        acceptor_.async_accept([this](asio::error_code ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                try
                {
                    auto port = socket.remote_endpoint().port();
                    auto address = socket.remote_endpoint().address().to_string();
                    spdlog::info("accept {}:{}", address, port);
                }
                catch (const std::exception& e)
                {
                    spdlog::warn(e.what());
                }
                auto conn = std::make_shared<Conn>(std::move(socket));
                conn->read();
                this->listen();
            }
            else {
                spdlog::info(ec.message());
            }
        });
    }

};


int main()
{
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l%$] %v");
    try
    {
        asio::io_context ioc{ 1 };
        asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](auto, auto) {ioc.stop(); });
        listener server(ioc);
        server.listen();
        ioc.run();
    }
    catch (const std::exception& e)
    {
        spdlog::warn(e.what());
    }
    return 0;
}
