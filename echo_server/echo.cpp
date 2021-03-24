#include <spdlog\spdlog.h>
#define ASIO_HAS_CO_AWAIT
#include <asio.hpp>
using namespace asio;

awaitable<void> echo(asio::ip::tcp::socket socket)
{
    try
    {
        char data[1024];
        while (true)
        {
            size_t n = co_await socket.async_read_some(asio::buffer(data), asio::use_awaitable);
            spdlog::info("read {} bytes.", n);
            size_t m = co_await asio::async_write(socket, asio::buffer(data, n), asio::use_awaitable);
            spdlog::info("write {} bytes.", m);
        }
    }
    catch (const std::exception& e)
    {
        // 比如断开连接
        spdlog::warn(e.what());
    }
}

awaitable<void> listener()
{
    //asio::executor executor = co_await asio::this_coro::executor;   // compile fail.
    auto executor = co_await asio::this_coro::executor;     // TODO 返回值是什么类型
    using namespace asio::ip;
    tcp::acceptor acceptor(executor, { tcp::v4(), 55555 });
    while (true)
    {
        spdlog::info("waiting for someone connect ...");
        tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable);
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
        //方式一：编译不报错，执行不报错，但永远进不去 process 函数体
        //process(std::move(socket));
        //方式二：执行函数体，但只能顺序接入客户端：接入下一个需要等上一个断开。
        //co_await process(std::move(socket));
        //方式三：正确的打开方式，新建协程
        asio::co_spawn(executor,
            [socket = std::move(socket)]() mutable {
            return echo(std::move(socket));
        }, asio::detached);
    }
}

int main()
{
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l%$] %v");
    try
    {
        asio::io_context ioc{ 1 };
        asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](auto, auto) {ioc.stop(); });
        asio::co_spawn(ioc, listener, asio::detached);
        ioc.run();
    }
    catch (const std::exception& e)
    {
        spdlog::warn(e.what());
    }
    return 0;
}
