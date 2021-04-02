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