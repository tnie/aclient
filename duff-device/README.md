
[深入理解达夫设备](https://mthli.xyz/duff-device/)

维基百科 https://en.wikipedia.org/wiki/Duff%27s_device 中明确提到 `/* count > 0 assumed */`，大多数中文博客介绍时却从不提及这个约定。有些人也是一知半解地搬运，不做测试张口就来。

点名批评 [什么是达夫设备（Duff's Device）](https://www.cnblogs.com/xkfz007/archive/2012/03/27/2420163.html)

> 为什么"case 0"标记在循环外面呢？这样不是打破了对称的美观吗？这样做的唯一理由是为了处理空序列。

我也不知道为什么，放在循环里面执行结果一样。


