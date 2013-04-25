highPerformanceNetworkLibrary
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
