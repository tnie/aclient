参考 v2ex 帖子 [修改了 UA 导致百度主动降级 http](https://www.v2ex.com/t/648384)

使用 https 协议，在定义 user-agent 后访问某些网站要充分测试。

[boost beast message](https://www.boost.org/doc/libs/develop/libs/beast/doc/html/beast/using_http/message_containers.html)


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

