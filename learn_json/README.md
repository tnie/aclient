从 [微服务开发：C++语言是否真的不适用？][1] 展开，了解其中的某些三方库。

REST 框架，提到的微软开源的 [cpprestsdk](https://github.com/microsoft/cpprestsdk)，也是奇怪：

> cpprestsdk is in maintenance mode and we do not recommend its use in new projects. We will continue to fix critical bugs and address security issues.
> 
> cpprestsdk 处于维护模式，我们不建议在新项目中使用它。我们将继续修复关键错误并解决安全问题。

使用 [RapidJSON](https://github.com/Tencent/rapidjson) 进行 JSON 解析和处理。

SAX 和 DOM 风格：

> Simple API for XML（简称 [SAX][sax]）是个循序访问XML的解析器API。SAX提供一个机制从XML文件读取资料。它是除了文档对象模型（[DOM][dom]）的另外一种流行选择。

暂时放弃了解 RapidJSON，因为虽然在作者的评测里表现突出，但 [api 的易用性上表现糟糕][2]。作者也承认：

> RapidJSON 也有一些缺点，例如有些 API 的设计比较奇怪，可能较难使用。


[1]:https://skyscribe.github.io/post/2019/10/01/microservice-with-cpp/
[sax]:https://zh.wikipedia.org/wiki/SAX
[dom]:https://zh.wikipedia.org/wiki/%E6%96%87%E6%A1%A3%E5%AF%B9%E8%B1%A1%E6%A8%A1%E5%9E%8B
[2]:https://www.zhihu.com/question/23654513