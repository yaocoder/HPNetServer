highPerformanceNetworkServer
=============================
-----
**目标**: 高性能TCP网络服务器程序  

**简介**: 程序架构采用[master-worker模型](http://yaocoder.blog.51cto.com/2668309/1170944)，并利用libevent网络库来实现one loop per thread(一个事件循环一个线程)的IO模型。  
  
  
* **支持平台**: x86-64 linux  
* **开发语言**: C++  
* **开发平台**: CentOS release 6.3 
* **linux内核版本**: 2.6.32-279.el6.x86_64 
* **gcc 版本**: 4.4.6
* **[libevent](http://libevent.org/)版本**: 2.0.21
* 

**测试结果**:

* 对于tcp短连接以简单的请求——回应进行测试，10万个请求，平均响应时间约为16ms。
* 对于tcp长连接，由于客户端测试资源限制，只测了1万个并发连接（加30s心跳），服务端可以轻松应对。


**参考文章**

* [网络编程释疑之：单台服务器上的并发TCP连接数可以有多少](http://yaocoder.blog.51cto.com/2668309/1312821)

* [网络编程释疑之：TCP半开连接的处理](http://yaocoder.blog.51cto.com/2668309/1309358)

* [网络编程释疑之：TCP的TIME_WAIT状态在服务器开发中的影响？](http://yaocoder.blog.51cto.com/2668309/1338567)

* [我的网络开发之旅——socket编程](http://yaocoder.blog.51cto.com/2668309/1556742)
