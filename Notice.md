##服务器配置及注意事项

1.**开启core dump**  

在/etc/profile文件最后添加
> \#open core dump  
> ulimit -S -c unlimited /dev/null 2>&1    

表示打开core dump，对dump文件的大小无限制,然后source /etc/profile 使之生效。

2.**配置最大文件打开数**

* 查看服务器的系统级打开文件数限制。
  cat /proc/sys/fs/file-max
当前业务服务器该数值为384469。
这表明这台Linux系统最多允许同时打开(即包含所有用户打开文件数总和) 384469个文件，是Linux系统级硬限制，所有用户级的打开文件数限制都不应超过这个数值。通常这个系统级硬限制是Linux系统在启动时根据系统硬件资源状况计算出来的最佳的最大同时打开文件数限制，如果没有特殊需要，不应该修改此限制。

* 修改用户级最大打开文件数,使用如下命令查看最大打开文件数。

 >  ulimit –Hn    (硬限制)  
 >  ulimit –Sn   （软限制）


在profile文件中添加ulimit修改。在/etc/profile文件最后添加  

> \#add the max filefds  
>  ulimit -SHn 384469

然后source /etc/profile 使之生效。



