#define ASIO_HAS_CO_AWAIT
#include <asio.hpp>
#include <spdlog\spdlog.h>

int main()
{
    asio::io_context ioc{ 1 };
    asio::co_spawn(ioc,
        [&ioc]() ->asio::awaitable<void> {
        auto executor = co_await asio::this_coro::executor;
        //spdlog::info(typeid(executor).name());
        //spdlog::info(typeid(asio::any_io_executor).name());
        spdlog::info("executor is {}an object of asio::any_io_executor type.",
            typeid(executor) == typeid(asio::any_io_executor) ? "" : "**not** ");
        asio::any_io_executor ex = ioc.get_executor();
        bool eq = (executor == ex);     // io_context ²»ÄÜ×ö == ÅÐ¶Ï
        if (eq)
        {
            auto* addr = &executor.context();
            bool same = (addr == &ioc);
            eq = same && eq;
        }
        spdlog::info("executor is {}ioc's executor.", eq ? "" : "**not** ");

    }, asio::detached);
    {
        size_t num = ioc.run();
        spdlog::info("{} handlers were executed", num);
    }
    return 0;
}