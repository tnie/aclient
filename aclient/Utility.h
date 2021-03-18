#pragma once
// 此文件限于只使用标准库
#include <string>
#include <iostream>
#include <chrono>
#include <memory>
#include <cassert>
#include <sstream>
#include <algorithm>
#include <cctype>

template<typename T>
class Passkey {
private:  // CAN'T BE public
    friend T;
    Passkey() {};   // can't use '=default'
    Passkey(const Passkey&) {};
    Passkey& operator=(const Passkey&) = delete;
};

class noncopyable
{
protected:
    noncopyable() = default;
    virtual ~noncopyable() = default;
private:
    noncopyable(const noncopyable&) = delete;
    noncopyable(noncopyable&&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
    noncopyable& operator=(noncopyable&&) = delete;
};

template <typename T>
class Singleton : public noncopyable
{
public:
    static T& getInstance()
    {
        static T instance;
        return instance;
    }
protected:
    Singleton() = default;
    virtual ~Singleton() = default;
};

template <typename T>
class SingletonPtr : public noncopyable
{
public:
    static std::shared_ptr<T> getInstance()
    {
        static std::shared_ptr<T> instance(new T);
        //static auto instance = std::make_shared<T>();    // maybe invalid if private/protected ctor
        // TODO 非线程安全？
        return instance;
    }
protected:
    SingletonPtr() = default;
    virtual ~SingletonPtr() = default;
};


class TickTick
{
public:
    TickTick() :
        t1(std::chrono::steady_clock::now()), tn(t1)
    {
    }
    ~TickTick()
    {
        auto t2 = std::chrono::steady_clock::now();
        std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - tn).count() << " milliseconds" << std::endl;
    }
    std::string tick(const std::string& msg = "")
    {
        auto t2 = std::chrono::steady_clock::now();
        std::ostringstream oss;
        oss << msg << " cost:" << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - tn).count() << " milliseconds";
        std::cout << oss.str() << std::endl;
        tn = t2;
        return oss.str();
    }
private:
    const std::chrono::steady_clock::time_point t1;
    std::chrono::steady_clock::time_point tn;
};

#include <numeric>
#include <vector>
namespace utility
{
    // NOTE 空集合时返回空字符串
    inline std::string joinItems(const std::vector<std::string>& x, const std::string & separator)
    {
        using std::string;
        return  std::accumulate(x.cbegin(), x.cend(), string(),
            [separator](const string& ss, const string &s)
        {
            return ss.empty() ? s : ss + separator + s;
        });
    }

    std::string uuid();

    std::string getProperty(const std::string key);

    //去除左侧空白字符
    std::string& ltrim(std::string& str);
    //去除右侧空白字符
    std::string& rtrim(std::string& str);
    std::string& trim(std::string& str);

    //去除前导 ch 字符
    std::string& ltrim(std::string& str, char ch);
    //去除末尾 ch 字符
    std::string& rtrim(std::string& str, char ch);
    std::string& trim(std::string& str, char ch);

    inline std::string tolower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) -> unsigned char { return std::tolower(c); });
        return s;
    }

    //输出格式 "%04d-%02d-%02d"
    std::string time2date(time_t tt);

    //输出格式 "%02d:%02d:%02d"
    std::string time2str(time_t tt, bool removeDate = false);
}

