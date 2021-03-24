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
        // ����Ͽ�����
        spdlog::warn(e.what());
    }
}

awaitable<void> listener()
{
    //asio::executor executor = co_await asio::this_coro::executor;   // compile fail.
    auto executor = co_await asio::this_coro::executor;     // TODO ����ֵ��ʲô����
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
        //��ʽһ�����벻����ִ�в���������Զ����ȥ process ������
        //process(std::move(socket));
        //��ʽ����ִ�к����壬��ֻ��˳�����ͻ��ˣ�������һ����Ҫ����һ���Ͽ���
        //co_await process(std::move(socket));
        //��ʽ������ȷ�Ĵ򿪷�ʽ���½�Э��
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
