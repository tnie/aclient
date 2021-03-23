#include <spdlog\spdlog.h>
#include <thread>
#include <chrono>
#define ASIO_HAS_CO_AWAIT
#include <asio.hpp>

using namespace asio;
using namespace std;
using namespace std::chrono_literals;

void deconstruction(asio::io_context& ioc)
{
    asio::steady_timer task(ioc, 500ms);
    // 同 cancel
    task.async_wait(
        [](const asio::error_code& ec) {
        if (asio::error::operation_aborted == ec) {
            spdlog::warn("asio::error::operation_aborted after deconstruction. \n{}\n{}",
                "~basic_waitable_timer() This function destroys the timer, cancelling any outstanding asynchronous"
                , " wait operations associated with the timer as if by calling");
        }
        else {
            spdlog::info("do something.");
        }
    });
    this_thread::sleep_for(100ms);
}

void completes_immediately(asio::io_context& ioc)
{
    asio::steady_timer task(ioc, 500ms);
    spdlog::info("waiting ...");
    std::this_thread::sleep_for(1s);
    // 同 post
    task.async_wait(
        [](const asio::error_code& ec) {
        if (asio::error::operation_aborted == ec) {
            spdlog::warn("asio::error::operation_aborted when completes immediately.");
        }
        else {
            spdlog::info("do something. \n{}\n{}",
                "On immediate completion, invocation of the handler will be performed",
                " in a manner equivalent to using asio::post()");
        }
    });
    //如果不延迟，则退化成 deconstruction() 场景
    this_thread::sleep_for(10ms);
}

void common(asio::steady_timer& task)
{
    task.async_wait(
        [](const asio::error_code& ec) {
        if (asio::error::operation_aborted == ec) {
            spdlog::warn("asio::error::operation_aborted @{}.", "common");
        }
        else {
            spdlog::info("do something @{}.", "common");
        }
    });
}

auto use_future(asio::steady_timer& task)
{
    return std::async(
        [&task]() {
        try
        {
            std::future<void> future = task.async_wait(asio::use_future);
            /*auto value = */future.get();
            spdlog::info("the future @{}.", "use_future");
        }
        catch (const asio::system_error& e)
        {
            if (asio::error::operation_aborted == e.code()) {
                spdlog::warn("asio::error::operation_aborted @{}.", "use_future");
            }
            else {
                spdlog::warn(e.what());
            }
        }
        catch (const std::exception& e)
        {
            spdlog::warn(e.what());
        }
    });
}

void use_detached(asio::steady_timer& task)
{
    task.async_wait(asio::detached);
}

void use_coro(asio::io_context& ioc, asio::steady_timer& task)
{
    asio::co_spawn(ioc,
        [&task]() ->asio::awaitable<void> {
        asio::error_code ec;
        co_await task.async_wait(asio::redirect_error(asio::use_awaitable, ec));
        if (asio::error::operation_aborted == ec) {
            spdlog::warn("asio::error::operation_aborted @{}.", "use_coro");
        }
        else {
            spdlog::info("hello coro @{}.", "use_coro");
        }
    }, asio::detached);
}

void use_coro_handler(asio::io_context& ioc, asio::steady_timer& task)
{
    // TODO 不能捕获协程内部的错误吗？如何抛出一个异常？如何正确使用？
    auto handler = [](std::exception_ptr eptr) {
        try {
            if (eptr) {
                std::rethrow_exception(eptr);
            }
            else {
                spdlog::info("hello coro @completion token");
            }
        }
        catch (const asio::system_error& e)
        {
            if (asio::error::operation_aborted == e.code()) {
                spdlog::warn("asio::error::operation_aborted @{}.", "use_coro");
            }
            else {
                spdlog::warn(e.what());
            }
        }
        catch (const std::exception& e) {
            spdlog::warn(e.what());
        }
    };
    asio::co_spawn(ioc,
        [&task]() ->asio::awaitable<void> {
        co_await task.async_wait(asio::use_awaitable);
        spdlog::info("hello coro");
        std::string().at(1);    // throw exception
    }, handler);
}

int main()
{
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l%$] %v");
    asio::io_context ioc{ 1 };
    //存在 signals 所以不需要 guard，两者二选一
    std::unique_ptr<asio::executor_work_guard<decltype(ioc.get_executor())> > guard = nullptr;
    // CTRL-C
    std::unique_ptr<asio::signal_set> signals = nullptr;
    if (false) {
        signals.reset(new asio::signal_set{ ioc, SIGINT, SIGTERM });
        signals->async_wait(
            [&ioc](auto, auto) {
            ioc.stop();
        });
    }
    else {
        guard.reset(new asio::executor_work_guard<decltype(ioc.get_executor())>{ ioc.get_executor() });
    }
    std::thread running(
        [&ioc]() {
        size_t num = ioc.run();
        spdlog::info("{} handlers were executed", num);
    });
    // 保证在 mgr 之后再释放 &&
    asio::steady_timer task(ioc, 1s);
    spdlog::info("waiting 1s ...");
    // RAII
    auto mgr = std::shared_ptr<std::thread>(&running,
        [&guard](std::thread * ptr) {
        if (guard) {
            guard->reset(); // not reset() ptr
        }
        if (ptr && ptr->joinable()) {
            ptr->join();
        }
    });
    // do something
    //deconstruction(ioc);
    //completes_immediately(ioc);
    //common(task);
    //auto f = ::use_future(task);
    use_coro_handler(ioc, task);
    this_thread::sleep_for(100ms);
    //task.cancel();
    return 0;
}