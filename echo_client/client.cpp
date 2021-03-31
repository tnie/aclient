#include <spdlog\spdlog.h>
#define ASIO_HAS_CO_AWAIT
#include <asio.hpp>
#include <thread>
#include <chrono>
#include "common.h"
using namespace asio;
using namespace std::chrono_literals;
long g_count = 0;

//每秒钟建立一个新连接，每个连接每秒读写一次

awaitable<void> echo(asio::ip::tcp::socket socket)
{
    auto executor = co_await asio::this_coro::executor;
    auto timer = asio::steady_timer(executor);
    unsigned port = 0;
    try
    {
        port = socket.local_endpoint().port();
    }
    catch (const std::exception& e)
    {
        spdlog::warn(e.what());
    }
    try
    {
        char data[1024];
        string msg;
        while (true)
        {
            std::swap(msg, random_string(1000));
            //std::this_thread::sleep_for(1s);
            timer.expires_after(1s);
            co_await timer.async_wait(asio::use_awaitable);
            size_t m = co_await asio::async_write(socket, asio::buffer(msg), asio::use_awaitable);
            spdlog::info("write {} bytes using @{}.", m, port);
            size_t n = co_await socket.async_read_some(asio::buffer(data), asio::use_awaitable);
            spdlog::info("read {} bytes using @{}.", n, port);

        }
    }
    catch (const std::exception& e)
    {
        // 比如断开连接
        spdlog::warn("{}  {}", e.what(), --g_count);
    }
}

awaitable<void> conn()
{
    auto executor = co_await asio::this_coro::executor;
    asio::ip::tcp::resolver resolver{ executor };
    using namespace asio::ip;
    auto endpoints = co_await resolver.async_resolve(tcp::v4(), "127.0.0.1", "55555", asio::use_awaitable);
    asio::ip::tcp::socket socket{ executor };
    co_await asio::async_connect(socket, endpoints, asio::use_awaitable);
    asio::co_spawn(executor, conn, asio::detached);
    std::this_thread::sleep_for(1s);
    co_await echo(std::move(socket));
}

int main()
{
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l%$] %v");
    try
    {
        asio::io_context ioc{ 1 };
        asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](auto, auto) {ioc.stop(); });
        asio::co_spawn(ioc, conn, asio::detached);
        ioc.run();
    }
    catch (const std::exception& e)
    {
        spdlog::warn(e.what());
    }
    return 0;
}
