#include <experimental\coroutine>
#include <iostream>
#include <cassert>

// ��ΪЭ�̵ķ���ֵ������ caller ��Э�̽������Զ������ͣ�������벻��ʹ�á�
class resumable {
public:
    struct promise_type {   // ������ӿ������ǹ̶��ģ����������Ҫʹ��
        using coro_handle = std::experimental::coroutine_handle<promise_type>;
        const char* string_;
        resumable get_return_object() {
            coro_handle handle = coro_handle::from_promise(*this);
            return resumable(handle);
        }
        auto initial_suspend() {
            return std::experimental::suspend_always();
        }
        auto final_suspend() {
            return std::experimental::suspend_always();
        }
        //void return_void() {}
        //co_return ���ʽ����
        void return_value(const char* string) {
            string_ = string;
        }
        //co_yield ���ʽ����
        auto yield_value(const char* string) {
            string_ = string;
            return std::experimental::suspend_always();
        }
        void unhandled_exception() {
            std::terminate();
        }
    };
    using coro_handle = std::experimental::coroutine_handle<promise_type>;

    resumable(coro_handle handle) : handle_(handle) { assert(handle); }
    //resumable(resumable&) = delete;
    //resumable(resumable&&) = delete;

    bool resume() {
        if (! handle_.done())
            handle_.resume();
        return ! handle_.done();
    }
    const char* return_val() {
        return handle_.promise().string_;
    }
    const char* recent_val() {
        return handle_.promise().string_;
    }
    ~resumable() { handle_.destroy(); }
private:
    coro_handle handle_;
};

resumable foo() {
    std::cout << "Hello" << std::endl;
    co_await std::experimental::suspend_always();
    std::cout << "World" << std::endl;
    co_return "Coroutine";
}

int main() {
    resumable res = foo();
    while (res.resume());
    std::cout << res.return_val() << std::endl;
}