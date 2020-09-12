#include "server.h"
#include "http_conn.h"
#include "thread_pool.h"
#include "connection_pool.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000
#define ASYNLOG

Server* Server::GetServer()
{
    static Server server;
    return &server;
}

int Server::Init(int argc,char** args)
{
    //Configure();

    #ifdef ASYNLOG
        Log::GetInstance()->init("ServerLog", 2000, 800000, 8); //异步日志模型
    #else
        Log::GetInstance()->init("ServerLog", 2000, 800000, 0); //同步日志模型
    #endif

    StartListen(argc,args);

    ConnectionPool* connpool=ConnectionPool::GetInstance();
    //init参数：ip，端口（0表示默认），数据库用户名，数据库密码，数据库名，连接池中连接个数
    connpool->init("localhost",0,"root","123","db",5);
    HttpConn::GetDataBase(connpool);

    requet_queue=new BlockQueue<HttpConn*>;
    ThreadPool* pool=nullptr;
    pool=new ThreadPool(connpool,requet_queue);//建立线程池

    users=new HttpConn[1000];
}

void Server::StartListen(int argc,char** args)
{
    in_addr ip;

    //创建IPv4 socket 地址
    struct sockaddr_in server_address;//定义服务端套接字
    bzero(&server_address,sizeof(server_address));//先将服务器套接字结构体置0
    server_address.sin_family=AF_INET;//然后对服务端套接字进行赋值：IPV4协议、本机地址、端口
    inet_pton(AF_INET,ip,&server_address.sin_addr);//point to net
    server_address.sin_port=htons(port);//host to net short

    //创建TCPsocket，并将其绑定到端口port上
    listenfd=socket(AF_INET,SOCK_STREAM,0);//指定协议族：IPV4协议，套接字类型：字节流套接字，传输协议类型：TCP传输协议，返回套接字描述符;
    if(listenfd==-1)
    {
        perror("create socket: ");
        exit(EXIT_FAILURE);
    }
    struct linger tmp={1,0};
    setsockopt(listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));

    int ret=bind(listenfd,(struct sockaddr*) &server_address,sizeof(server_address));//将监听描述符与服务器套接字绑定
    if(ret==-1)
    {
        perror("bind socket");
        exit(EXIT_FAILURE);
    }
    
    ret=listen(listenfd,5);//将主动套接字转换为被动套接字，并指定established队列上限
    if(ret==-1)
    {
        perror("listen socketfd");
        exit(EXIT_FAILURE);
    }
    LOG_INFO("will use host ip %s and port:1234 as server socket address",point_ip);
}

void Server::EventLoop()
{

    epoll_event events[MAX_EVENT_NUMBER];
    int epfd=epoll_create(10);

    bool stop_server=false;
    bool time_out=false;

    while(!stop_server)
    {
        int number=epoll_wait(epfd,events,MAX_EVENT_NUMBER,-1);
        if((number<0) && (errno!=EINTR))
        {
            perror("epoll failed");
            exit(EXIT_FAILURE);
        }
        for(int i=0;i<number;i++)
        {
            int fd=events[i].data.fd;
            if(fd==listenfd && events[i].events & EPOLLIN)
                AcceptCallback();
            else if(fd==pipefd && events[i].events & EPOLLIN)
                SignalCallbcak();
            else if(events[i].events & (EPOLLHUP | EPOLLERR))
                CloseCallback();
            else if(events[i].events & EPOLLIN)
                ReadCallback();
            else if(events[i].events & EPOLLOUT)
                WriteCallback();
            else 
                continue;
        }
    }
}