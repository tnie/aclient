#include <spdlog\spdlog.h>

namespace
{
    constexpr auto __TMP__ = 10e8;
    static_assert(__TMP__ < std::numeric_limits<size_t>::max(), "");
    constexpr size_t MAX = static_cast<size_t>(__TMP__);
}

// ִ����Σ���ӡ���ʱ
void run(std::function<void(void)> callable)
{
    auto t1 = std::chrono::system_clock::now();
    if (callable)
    {
        callable();
    }
    auto t2 = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
    spdlog::info("{} costs {} milliseconds.", typeid(callable).name(), duration.count());
}

void func()
{
    long long sum = 0;
    spdlog::info("start {}", __FUNCDNAME__);
    for (size_t i = 0; i < MAX; i++)
    {
        sum ++;
    }
    if (sum!=MAX)
    {
        spdlog::error("{} != {}", sum, MAX);
    }
}

// ѭ��չ��
void unroll()
{
    long long sum = 0;
    static_assert(MAX % 5 == 0, "");
    spdlog::info("start {}", __FUNCDNAME__);
    for (size_t i = 0; i < MAX; i+=5)
    {
        sum ++;
        sum ++;
        sum ++;
        sum ++;
        sum ++;
    }
    if (sum != MAX)
    {
        spdlog::error("{} != {}", sum, MAX);
    }
}

int main()
{
    // Release �汾��Ҫ���ñ�������ѭ��չ��������Ż�
    run(func);
    run(unroll);
    return 0;
}