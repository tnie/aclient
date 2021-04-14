参考 v2ex 帖子 [修改了 UA 导致百度主动降级 http](https://www.v2ex.com/t/648384)

使用 https 协议，在定义 user-agent 后访问某些网站要充分测试。

# 类型 `message`

[boost beast message][message] 笔记

```cpp
/// A typical HTTP request
template<class Body, class Fields = fields>
using request = message<true, Body, Fields>;

/// A typical HTTP response
template<class Body, class Fields = fields>
using response = message<false, Body, Fields>;
```


```cpp
template<
    bool isRequest,
    class Body,
    class Fields = fields>
class message :
    public http::header< isRequest, Fields >

```

The `Body` template argument type determines the model used to read or write the content body of the message.


```cpp
template<
    bool isRequest,
    class Fields = fields>
class header :
    public Fields
```

A header includes the **start-line** and header-fields.

`using fields = basic_fields< std::allocator< char > >;`

This container is designed to store the field value pairs that make up the fields and trailers in an HTTP message. 

Field names are stored as-is, but comparisons are case-insensitive. 不区分大小写

When the container is iterated the fields are presented in the order of insertion, with fields having the same name following each other consecutively. 

# 协程版本

写协程版本的两种思路：

- 学些阶段，可以用异步回调形式改协程版本；
- 最终目的，是要写同步版本的代码：直接写同步版本的代码，然后加 `co_await` 变协程版本。

如果每次写协程，都是由异步回调版本改动而来。岂不是本末倒置？我们用协程的目的是什么？

协程版本和异常更搭配，但异步接口都是 `erro_code` 版本的（同步接口才存在异常和 `error_code` 两种错误处理方式），封 awaitable/awaiter 的是否内部做了抛出？

面对 eof 或短读等错误时，调用 `redirect_error()` 和局部捕获异常两种方式，哪种更值得推荐？


[message]:https://www.boost.org/doc/libs/develop/libs/beast/doc/html/beast/using_http/message_containers.html