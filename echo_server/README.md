源码来源：

https://www.boost.org/doc/libs/1_70_0/doc/html/boost_asio/example/cpp17/coroutines_ts/echo_server.cpp

上述（使用单线程&协程）版本使用 jmeter 进行压测：[JMeter TCP性能测试][1]

因为程序简单，开到 20000 个连接并发没有问题，三万个可能也没问题，但 jmeter 开三万总是崩溃所以没测试出结果。

推测配置某些参数应该能够将并发连接数提升上来，暂缓。

而“每个连接一个线程”的版本 https://think-async.com/Asio/asio-1.18.1/src/examples/cpp11/echo/blocking_tcp_echo_server.cpp ，同时并发 1600 个连接时就会崩溃。差距明显。

使用单一机器、单一编译器的 Debug 模式测试。后者 Release 模式也撑不住 1600 个并发连接。


[1]:https://www.cnblogs.com/youxin/p/8684891.html