# 微型web服务器程序（接近原始版本）

**C++ & HTML**


这是一个微型web服务器程序，大部分代码参照了游双的《Linux高性能服务器编程》，另外一部分参照了[TinyWebServer](https://github.com/qinguoyi/TinyWebServer "TinyWebServer")和公众号两猿社的文章[Web服务器详解](https://mp.weixin.qq.com/mp/appmsgalbum?__biz=MzAxNzU2MzcwMw==&action=getalbum&album_id=1339230165934882817&subscene=159&subscene=&scenenote=https%3A%2F%2Fmp.weixin.qq.com%2Fs%2FBfnNl-3jc_x5WPrWEJGdzQ#wechat_redirect "Web服务器详解"),包括数据库连接池、阻塞队列和日志等。另外加上了我自己对于程序的理解和一些改进。


## 使用方法

### 环境

- Ubuntu 18.04
- MySQL 5.7.31

### 修改程序

- 将main.cc中数据库连接池初始化参数修改为你自己的数据库用户名、用户密码和数据库名称

`connpool->init("localhost",0,"root","123","db",5);`

- 将http_conn.cc文件中的html文件所在的根目录改成你系统中该root文件夹所在路径

`const char* doc_root="/home/sing/code/fight/my_web_server/root";`

### 编译

在Makefile所在文件夹运行命令行

`make`

### 运行

在程序所在文件夹运行命令行,其中ip改成你电脑的IP，端口改成任意不冲突端口即可（不用加[]）

`./server [your ip] [port]`

## 服务器架构
**半同步半反应堆式：主线程负责接受新连接和网络数据读写，工作线程只负责解析和处理客户请求**

### 使用到的技术
- 定时器（基于升序链表）
- 日志（基于阻塞队列）
- 线程池
- 线程间通信：信号量、互斥锁、条件变量
- 数据库和连接池
- I/O复用（epoll）
- 统一事件源（socketpair管道）

### 结构
#### 主线程 epoll监听所有事件
- 连接描述符可读->初始化该连接描述符对应的客户对象，添加该连接的定时器
- 异常事件->关闭连接
- 信号管道可读
 - 终止信号->将终止服务器标志设为true
 - SIGALRM信号->将timeout标志设为true，所有就绪事件处理完毕后将调用定时器的Tick函数，清理到期的定时器
- 连接描述符可读-> 
	1. 主线程读取客户数据，放在客户对象中
	2. 将客户对象指针放入请求队列中
	3. 调整定时器
- 连接描述符可写->将客户对象中写缓冲区的数据发送给客户端

#### 工作线程 
1. 阻塞等待请求到来
2. 从请求队列中取出客户对象指针，对其中的读缓冲区内容进行解析和处理
3. 将响应写入客户对象的写缓冲区中，使epoll改为监听该连接描述符上的可写事件