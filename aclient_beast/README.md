参考 v2ex 帖子 [修改了 UA 导致百度主动降级 http](https://www.v2ex.com/t/648384)

使用 https 协议，在定义 user-agent 后访问某些网站要充分测试。

# 类型 `message`

[boost beast message][message] 笔记

request 或者 response 都是 message

```cpp
/// A typical HTTP request
template<class Body, class Fields = fields>
using request = message<true, Body, Fields>;

/// A typical HTTP response
template<class Body, class Fields = fields>
using response = message<false, Body, Fields>;
```

message 继承于 fields，后者是 [field][f] 的容器

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

# SSL

源码地址：

https://www.boost.org/doc/libs/1_75_0/libs/beast/example/http/client/async-ssl/http_client_async_ssl.cpp

以下关于 SSL/TLS 内容摘自维基百科

传输层安全性协议（TLS）及其前身安全套接层（SSL）是一种安全协议，目的是为互联网通信提供安全及数据完整性保障。网景公司（Netscape）在1994年推出首版网页浏览器－网景导航者时，推出HTTPS协议，以SSL进行加密，这是SSL的起源。IETF将SSL进行标准化，

- 1999年公布TLS 1.0标准文件（RFC 2246）。
- 随后又公布TLS 1.1（RFC 4346，2006年）、
- TLS 1.2（RFC 5246，2008年）和
- TLS 1.3（RFC 8446，2018年）。

| 协议	|发布时间	    |状态|
|------|--------------|-----------|
| SSL 1.0	|未公布	|未公布|
| SSL 2.0	|1995年	|已于2011年弃用[2]|
| SSL 3.0	|1996年	|已于2015年弃用[3]|
| TLS 1.0	|1999年	|于2021年弃用[4]  |
| TLS 1.1	|2006年	|于2021年弃用[4]  |
| TLS 1.2	|2008年	|                |
| TLS 1.3	|2018年	|                |


关于 `SSL_VERIFY_PEER` 的介绍，参考 [SSL_CTX_set_verify](https://www.openssl.org/docs/man1.0.2/man3/SSL_CTX_set_verify.html)

为什么选择 `SSL_VERIFY_PEER` 就无法通信呢？是本地网络环境或者网络服务提供商的锅吗？

不显示指定 CA，那默认使用的哪里的 CA 呢？


## openssl

[OpenSSL 1.1.0+ have changed library names in windows](https://github.com/tnie/StockDataLayer/issues/9)

在 windows 平台使用 openssl 有两种方式：

1. 下载源码，根据其中的手册通过 nmake 生成动态库 or 静态库。需要 activeperl 和 nasm 环境。

	静态库或导入库名称 libeay32.lib，ssleay32.lib；动态库名称 libeay32.dll，ssleay32.dll。给一篇 [博客][4] 参考。

2. 从网上下载 [第三方编译的结果](https://wiki.openssl.org/index.php/Binaries)

	其中 grpc 同时也在用的 https://slproweb.com/products/Win32OpenSSL.html 此地址下载内容 VC 目录中文件以 `libcrypto32M[TD](d).lib` `libssl32M[TD](d).lib` 命名。




[f]:https://www.boost.org/doc/libs/1_75_0/libs/beast/doc/html/beast/ref/boost__beast__http__field.html
[message]:https://www.boost.org/doc/libs/develop/libs/beast/doc/html/beast/using_http/message_containers.html
