使用 asio **异步方式**进行简单的 http get/post 请求。特点：

- 使 HTTPRequest 对象可选地支持 https 通信
- 内部吞掉了 `SSL_R_SHORT_READ` 错误，打印 warning 日志，但不对外报错
- 针对  weak etag 做了简单处理
- 每个 task 对应一个 request，使用一个 socket，不支持复用
- request 支持 cancel


从工作项目独立出来的，经过实践存在以下问题：

- 为了不处理 `chunked`，将 http 协议限制在 http1.0
- 通信出现 `SSL_R_SHORT_READ` 是不是错误
- 工作中处理了 gzip 通信，但此处未引入 gzip 库

> 分块传输编码只在HTTP协议1.1版本（HTTP/1.1）中提供。 [维基百科](https://zh.wikipedia.org/wiki/%E5%88%86%E5%9D%97%E4%BC%A0%E8%BE%93%E7%BC%96%E7%A0%81)

公司运维的同事反馈服务器即便启用 gzip 通信，还依赖文件后缀名过滤是否真的压缩。

在业务场景中，下载文件存在失败的案例。之后更换了使用 libcurl 的 curlpp 实现文件下载。 

# co_wait

尝试过使用协程改写，但如何兼容 `cancel()` 接口呢？