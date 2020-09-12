#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cassert>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <netdb.h>

#include "thread_pool.h"
#include "http_conn.h"
#include "time/timer_container.h"
#include "connection_pool.h"
#include "log.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000
#define ASYNLOG


static int sigpipe[2];
extern int Addfd(int epollfd,int fd,bool oneshot);
extern int Removdfd(int epollfd,int fd);
extern int SetNonBlocking(int fd);
bool timeout =false;//通过该标志位来判断是否收到SIGALRM信号
bool is_reactor=false;

void AddSig(int sig,void (*handler) (int),bool restart=true)//设置给定信号对应的信号处理函数
{
    struct sigaction sa;//因为是用sigaction函数设置信号处理函数，所以需要先定义sigaction结构体
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=handler;//本程序中所有指定信号的信号处理函数都是上面这个，即将信号值写入管道
    if(restart)
        sa.sa_flags |= SA_RESTART;//当系统调用被信号中断时，在处理完信号后，是不会回到原来的系统调用的。加上了restart就会在信号处理函数结束后重新调用被该信号终止的系统调用
    sigfillset(&sa.sa_mask);//将信号掩码sa_mask设置为所有信号，即屏蔽所有除了指定的sig，也就是说信号处理函数只接收指定的sig
    assert(sigaction(sig,&sa,nullptr) != -1);//将信号sig的信号处理函数设置为sa中的sa_handler，并检测异常
}

void SigHandler(int sig)
{
    int save_errno=errno;
    int msg=sig;
    send(sigpipe[1],(char*) &msg,1,0);
    errno=save_errno;
}

void ShowError(int connfd,const char* info)
{
    LOG_INFO("server is busy,connection %d will be closed",connfd);
    send(connfd,info,strlen(info),0);
    close(connfd);
}

int main(int argc,char *argv[])
{
    /*if(daemon(1,0) == -1)
    {
        perror("daemon error");
        exit(EXIT_FAILURE);
    }*/
#ifdef ASYNLOG
    Log::GetInstance()->init("ServerLog", 2000, 800000, 8); //异步日志模型
#else
    Log::GetInstance()->init("ServerLog", 2000, 800000, 0); //同步日志模型
#endif

    in_addr ip;
    char point_ip[50];
    int port;
    if(argc<=2)
    {
        //printf("usage: %s ip_address port_number\n",basename(argv[0]));
        //return 1;
        char name[256]={0};
        if(gethostname(name,sizeof(name)))
        {
            perror("get host name failed");
            exit(EXIT_FAILURE);
        }
        struct hostent* host_entry=gethostbyname(name);
        if(host_entry)
        {
            ip=*((struct in_addr*)host_entry->h_addr_list[0]);
            port=1234;
        }
        else
        {
            perror("get host entry failed");
            exit(EXIT_FAILURE);
        }
        inet_ntop(AF_INET,host_entry->h_addr_list[0],point_ip,INET_ADDRSTRLEN);
        if(strncmp(point_ip,"127",3)==0)
            inet_pton(AF_INET,"172.21.0.5",&ip);
    }
    else
    {
        inet_aton(argv[1],&ip);
        port=atoi(argv[2]);
    }
    inet_ntop(AF_INET,&ip,point_ip,INET_ADDRSTRLEN);
    LOG_INFO("will use host ip %s and port:1234 as server socket address",point_ip);

    //创建IPv4 socket 地址
    struct sockaddr_in server_address;//定义服务端套接字
    bzero(&server_address,sizeof(server_address));//先将服务器套接字结构体置0
    server_address.sin_family=AF_INET;//然后对服务端套接字进行赋值：IPV4协议、本机地址、端口
    server_address.sin_addr=ip;
    //inet_pton(AF_INET,ip,&server_address.sin_addr);//point to net
    server_address.sin_port=htons(port);//host to net short

    ConnectionPool* connpool=ConnectionPool::GetInstance();
    //init参数：ip，端口（0表示默认），数据库用户名，数据库密码，数据库名，连接池中连接个数
    connpool->init("localhost",0,"root","123","db",5);

    ThreadPool<HttpConn>* pool=nullptr;
    pool=new ThreadPool<HttpConn> (connpool);//建立线程池

    TimerContainer timer_container(HEAP);//定时器容器
    HttpConn* users=new HttpConn[MAX_FD];//建立HTTP客户对象数组
    assert(users);
    users[0].GetDataBase(connpool);
    int user_count=0;

    //创建TCPsocket，并将其绑定到端口port上
    int listenfd=socket(AF_INET,SOCK_STREAM,0);//指定协议族：IPV4协议，套接字类型：字节流套接字，传输协议类型：TCP传输协议，返回套接字描述符;
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

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd=epoll_create(10);
    assert(epollfd!=1);
    Addfd(epollfd,listenfd,false);
    HttpConn::m_epollfd=epollfd;

    //ret=pipe(sigpipe);//pipe只能用于进程间通信，这里只有一个进程，
    //虽然一开始会往管道写SIGALARM信号，但epoll并不能检测到该管道可读，这就是为什么不会按时关闭连接的原因
    ret=socketpair(PF_UNIX,SOCK_STREAM,0,sigpipe);//而socketpair就没这个问题
    assert(ret!=-1);
    SetNonBlocking(sigpipe[1]);
    Addfd(epollfd,sigpipe[0],false);
    
    AddSig(SIGPIPE,SIG_IGN);
    AddSig(SIGALRM,SigHandler);
    AddSig(SIGTERM,SigHandler);
    AddSig(SIGINT,SigHandler);

    alarm(TIMESLOT);//设置闹钟
    bool stop_server=false;

    while(!stop_server)
    {
        int number=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);        
        if((number<0) && (errno!=EINTR))
        {
            perror("epoll failed");
            exit(EXIT_FAILURE);
        }
        for(int i=0;i<number;i++)//就绪事件：监听描述符可读、信号事件、连接描述符可读、连接描述符可写
        {
            int sockfd=events[i].data.fd;//取出每个就绪事件对应的文件描述符
            if(sockfd==listenfd)
            {//
                struct sockaddr_in client_address;//定义客户端套接字，accept函数将会把客户端套接字存放在该变量中
                socklen_t client_addr_length=sizeof(client_address);
                int connection_fd=accept(listenfd,(struct sockaddr*) &client_address,&client_addr_length);
                if(connection_fd<0)
                {
                    LOG_ERROR("connection accept failed ,errno is : %d",errno);
                    continue;
                }
                if(HttpConn::m_user_count>=MAX_FD)
                {
                    ShowError(connection_fd,"Internet server busy");
                    LOG_INFO("%s","connection number arrived max capacity");
                    continue;
                }
                users[connection_fd].Init(connection_fd,client_address);
                users[connection_fd].timer=timer_container.AddTimer(&users[connection_fd],3*TIMESLOT);//将该定时器节点加到链表中
                LOG_INFO("accept connection from port %d",ntohs(client_address.sin_port));
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLERR))
            {//连接被对方关闭、管道的写端关闭、错误
                LOG_INFO("EPOLLRDHUP || EPOLLERR,connection %d was closed by client",ntohs(users[sockfd].m_address.sin_port));
                timer_container.DeleteTimer(users[sockfd].timer);
                users[sockfd].CloseConn();
            }
            else if(sockfd==sigpipe[0] && events[i].events&EPOLLIN)
            {
                int sig;
                char signals[1024];
                ret=recv(sigpipe[0],signals,sizeof(signals),0);
                if(ret==-1)
                    continue;
                else if(ret==0)
                    continue;
                else
                {
                    for(int i=0;i<ret;++i)
                    {
                        switch(signals[i])
                        {
                            case SIGTERM:
                            case SIGINT:
                                LOG_INFO("%s","recieve terminate signal,server is goint to stop soon");
                                stop_server=true;
                                break;
                            case SIGALRM:
                                //timeout=true;
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
            else if(events[i].events & EPOLLIN)
            {
                #ifdef REACTOR
                {
                    pool->Append(users+sockfd);
                    timer_container.AdjustTimer(users[sockfd].timer,3*TIMESLOT);
                }
                #else
                {
                    if(users[sockfd].Read())//成功读取到HTTP请求
                    {
                        pool->Append(users+sockfd);//将该请求加入到工作队列中
                        timer_container.AdjustTimer(users[sockfd].timer,3*TIMESLOT);
                    }
                    else
                    {
                        timer_container.DeleteTimer(users[sockfd].timer);
                        users[sockfd].CloseConn();//读失败，关闭连接
                    }
                }
                #endif
            }
            else if(events[i].events & EPOLLOUT)
            {
                #ifdef REACTOR
                {
                    pool->Append(users+sockfd);
                    timer_container.AdjustTimer(users[sockfd].timer,3*TIMESLOT);
                }
                #else
                {
                    if(!users[sockfd].Write())//写失败或者是短连接
                    {
                        timer_container.DeleteTimer(users[sockfd].timer);
                        users[sockfd].CloseConn();//关闭连接
                    }
                    else
                        timer_container.AdjustTimer(users[sockfd].timer,3*TIMESLOT);
                }
                #endif
                
            }
            else 
                continue;
        }
        if(timeout)
        {
            timer_container.Tick();//处理到期的定时器
            timeout=false;
        }
    }

    close(epollfd);
    close(listenfd);//main函数创建的监听描述符，就必须由main函数来关闭
    close(sigpipe[0]);
    close(sigpipe[1]);
    delete []users;
    delete pool;
    return 0;
}