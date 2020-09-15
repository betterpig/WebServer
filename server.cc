#include "server.h"
#include "thread_pool.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000

void SigHandler(int sig)
{
    int save_errno=errno;
    int msg=sig;
    send(Server::GetServer()->sigpipe[1],(char*) &msg,1,0);
    errno=save_errno;
}

Server* Server::GetServer()
{
    static Server server;
    return &server;
}

int Server::Init(int argc,char** args)
{
    //Configure();
    connpool=ConnectionPool::GetInstance();
    //init参数：ip，端口（0表示默认），数据库用户名，数据库密码，数据库名，连接池中连接个数
    connpool->init("localhost",0,"root","123","db",5);
    HttpConn::GetDataBase(connpool);

    timer_container=new TimerContainer(WHEEL);//定时器容器
    HttpConn::timer_container=timer_container;
    requet_queue=new BlockQueue<HttpConn*>;
    thread_pool=new ThreadPool(connpool,requet_queue);//建立线程池
    users=new HttpConn[MAX_FD];

    StartListen(argc,args);
}

Server::~Server()
{
    close(epollfd);
    close(listenfd);//main函数创建的监听描述符，就必须由main函数来关闭
    close(sigpipe[0]);
    close(sigpipe[1]);

    delete thread_pool;
    delete timer_container;
    delete requet_queue;
    delete []users;
    printf("main thread %d exit\n",pthread_self());
    LOG_INFO("main thread %d exit",pthread_self());
}

void Server::StartListen(int argc,char** argv)
{
    const char* ip=argv[1];
    int port=atoi(argv[2]);

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

    ret=socketpair(PF_UNIX,SOCK_STREAM,0,sigpipe);//而socketpair就没这个问题
    assert(ret!=-1);
    AddSig(SIGPIPE,SIG_IGN);
    AddSig(SIGALRM,SigHandler);
    AddSig(SIGTERM,SigHandler);
    AddSig(SIGINT,SigHandler);

    epollfd=epoll_create(10);
    assert(epollfd!=1);
    Addfd(listenfd,false);
    SetNonBlocking(sigpipe[1]);
    Addfd(sigpipe[0],false);
}

void Server::EventLoop()
{
    epoll_event events[MAX_EVENT_NUMBER];
    bool stop_server=false;
    bool time_out=false;

    while(!stop_server)
    {
        int number=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
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
            else if(fd==sigpipe[0] && events[i].events & EPOLLIN)
                SignalCallback(stop_server,time_out);
            else if(events[i].events & (EPOLLHUP | EPOLLERR))
                CloseCallback(fd);
            else if(events[i].events & EPOLLIN)
                ReadCallback(fd);
            else if(events[i].events & EPOLLOUT)
                WriteCallback(fd);
            else 
                continue;
        }
    }
}

void Server::AcceptCallback()
{
    struct sockaddr_in client_address;//定义客户端套接字，accept函数将会把客户端套接字存放在该变量中
    socklen_t client_addr_length=sizeof(client_address);
    int connection_fd=0;
    while(true)
    {
        connection_fd=accept(listenfd,(struct sockaddr*) &client_address,&client_addr_length);
        if(connection_fd<0)
        {
            if(errno==EAGAIN || errno==EWOULDBLOCK)//这两个错误码表明数据读取结束
                break;
            else
            {
                LOG_ERROR("connection accept failed ,errno is : %d",errno);
                return;
            }
        }
        if(HttpConn::m_user_count>=MAX_FD)
        {
            LOG_INFO("server is busy,connection %d will be closed",connection_fd);
            char info[]="server is busy,connection refused";
            send(connection_fd,info,strlen(info),0);
            close(connection_fd);
            return;
        }
        //如下两行是为了避免timewait状态，仅用于调试，实际使用时应该去掉
        int reuse=1;
        setsockopt(connection_fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
        Addfd(connection_fd,true);
        users[connection_fd].Init(connection_fd,client_address);
        users[connection_fd].timer=timer_container->AddTimer(&users[connection_fd],3*TIMESLOT);//将该定时器节点加到链表中
    }
}

void Server::SignalCallback(bool& stop_server, bool& timeout)
{
    int sig;
    char signals[1024];
    int ret=recv(sigpipe[0],signals,sizeof(signals),0);
    if(ret==-1)
        return;
    else if(ret==0)
        return;
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
                timeout=true;
                break;
            default:
                break;
            }
        }
    }
}

void Server::CloseCallback(int sockfd)
{
    LOG_INFO("EPOLLRDHUP || EPOLLERR,connection %d was closed by client",ntohs(users[sockfd].m_address.sin_port));
    timer_container->DeleteTimer(users[sockfd].timer);
    users[sockfd].CloseConn();
}

void Server::ReadCallback(int sockfd)
{
    #ifdef REACTOR
    {
        requet_queue->push(users+sockfd);
    }
    #else
    {
        if(users[sockfd].Read())//成功读取到HTTP请求
        {
            requet_queue->push(users+sockfd);//将该请求加入到工作队列中
            timer_container->AdjustTimer(users[sockfd].timer,3*TIMESLOT);
        }
        else
        {
            timer_container->DeleteTimer(users[sockfd].timer);
            users[sockfd].CloseConn();//读失败，关闭连接
        }
    }
    #endif
}

void Server::WriteCallback(int sockfd)
{
    #ifdef REACTOR
    {
        requet_queue->push(users+sockfd);
    }
    #else
    {
        if(!users[sockfd].Write())//写失败或者是短连接
        {
            timer_container->DeleteTimer(users[sockfd].timer);
            users[sockfd].CloseConn();//关闭连接
        }
        else
            timer_container->AdjustTimer(users[sockfd].timer,3*TIMESLOT);
    }
    #endif
}

void Server::AddSig(int sig,void (*handler) (int),bool restart)//设置给定信号对应的信号处理函数
{
    struct sigaction sa;//因为是用sigaction函数设置信号处理函数，所以需要先定义sigaction结构体
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=handler;//本程序中所有指定信号的信号处理函数都是上面这个，即将信号值写入管道
    if(restart)
        sa.sa_flags |= SA_RESTART;//当系统调用被信号中断时，在处理完信号后，是不会回到原来的系统调用的。加上了restart就会在信号处理函数结束后重新调用被该信号终止的系统调用
    sigfillset(&sa.sa_mask);//将信号掩码sa_mask设置为所有信号，即屏蔽所有除了指定的sig，也就是说信号处理函数只接收指定的sig
    assert(sigaction(sig,&sa,nullptr) != -1);//将信号sig的信号处理函数设置为sa中的sa_handler，并检测异常
}

int Server::SetNonBlocking(int fd)//将文件描述符设为非阻塞状态
{
    int old_option = fcntl(fd,F_GETFL);//获取文件描述符旧的状态标志
    int new_option= old_option | O_NONBLOCK;//定义新的状态标志为非阻塞
    fcntl(fd,F_SETFL,new_option);//将文件描述符设置为新的状态标志-非阻塞
    return old_option;//返回旧的状态标志，以便日后能够恢复
}

void Server::Addfd(int fd,bool oneshot)//往内核事件表中添加需要监听的文件描述符
{
    epoll_event event;//定义epoll_event结构体对象
    event.data.fd=fd;
    event.events=EPOLLIN | EPOLLET | EPOLLRDHUP;//设置为ET模式：同一就绪事件只会通知一次
    if(oneshot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);//往该内核时间表中添加该文件描述符和相应事件
    SetNonBlocking(fd);//因为已经委托内核时间表来监听事件是否就绪，所以该文件描述符可以设置为非阻塞
}

void Server::Removefd(int fd)//将文件描述符fd从内核事件表中移除，并关闭该文件
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    shutdown(fd, SHUT_RDWR);
    close(fd);
}

void Server::Modfd(int fd,int ev)//在内核事件表中修改该文件的注册事件
{
    epoll_event event;
    event.data.fd=fd;
    event.events=ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;//并上给定事件，ET、ONESHOT、RDHUP（TCP连接被对方关闭）这三个一直有
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}