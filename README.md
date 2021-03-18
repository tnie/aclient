使用 asio **异步方式**进行简单的 http get/post 请求。

从工作项目独立出来的，经过实践存在以下问题：

- 为了不处理 `chunked`，将 http 协议限制在 http1.0
- 通信出现 `SSL_R_SHORT_READ` 是不是错误

> 分块传输编码只在HTTP协议1.1版本（HTTP/1.1）中提供。 [维基百科](https://zh.wikipedia.org/wiki/%E5%88%86%E5%9D%97%E4%BC%A0%E8%BE%93%E7%BC%96%E7%A0%81)

在业务场景中，下载文件存在失败的案例。之后更换了使用 libcurl 的 curlpp 实现文件下载。 
