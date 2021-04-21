#include <spdlog\spdlog.h>

namespace
{
    constexpr auto __TMP__ = 10e8;
    static_assert(__TMP__ < std::numeric_limits<size_t>::max(), "");
    constexpr size_t MAX = static_cast<size_t>(__TMP__);
}

// 执行入参，打印其耗时
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

// 循环展开
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

void unroll8(int count)
{
    if (count <=0)
    {
        //https://en.wikipedia.org/wiki/Duff%27s_device
        throw std::runtime_error("count > 0 assumed");
    }
    long long sum = 0;
    spdlog::info("start {}", __FUNCDNAME__);
    int loop = (count + 7) / 8;
    switch (const auto val = count % 8)
    {
    case 0:
        do {
            sum++;
    case 7:
        sum++;
    case 6:
        sum++;
    case 5:
        sum++;
    case 4:
        sum++;
    case 3:
        sum++;
    case 2:
        sum++;
    case 1:
        sum++;
        } while (--loop > 0);
    default:
        break;
    }

    if (sum != count)
    {
        spdlog::error("{} != {}", sum, count);
    }
}

int main()
{
    unroll8(5);
    unroll8(8);
    unroll8(10);
    // Release 版本需要禁用编译器在循环展开方面的优化
    run(func);
    run(unroll);
    return 0;
}