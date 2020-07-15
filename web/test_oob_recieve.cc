//测试带外数据的接收和发送

#include "sys/socket.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define BUF_SIZE 1024

int main(int argc,char* argv[])
{
    if(argc<=2)
    {
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
        return 1;
    }
    const char *ip=argv[1];
    int port=atoi(argv[2]);

    //创建IPv4 socket 地址
    struct sockaddr_in address;//定义服务端套接字
    bzero(&address,sizeof(address));//先将服务器套接字结构体置0
    address.sin_family=AF_INET;//然后对服务端套接字进行赋值：IPV4协议、本机地址、端口
    inet_pton(AF_INET,ip,&address.sin_addr);//point to net
    address.sin_port=htons(port);//host to net short

    int listenfd=socket(AF_INET,SOCK_STREAM,0);//指定协议族：IPV4协议，套接字类型：字节流套接字，传输协议类型：TCP传输协议，返回套接字描述符;
    assert(listenfd>=0);

    int ret=bind(listenfd,(struct sockaddr*) &address,sizeof(address));//将监听描述符与服务器套接字绑定
    assert(ret!=1);

    ret=listen(listenfd,5);//将主动套接字转换为被动套接字，并指定established队列上限
    assert(ret!=-1);
    
    struct sockaddr_in client_address;//定义客户端套接字，accept函数将会把客户端套接字存放在该变量中
    socklen_t client_addr_length=sizeof(client_address);
    int connection_fd=accept(listenfd,(struct sockaddr*) &client_address,&client_addr_length);
    if(connection_fd<0)
        printf("errno is: %d\n",errno);
    else
    {
        char buffer[BUF_SIZE];

        memset(buffer,'\0',BUF_SIZE);//用空字符清零buffer
        ret=recv(connection_fd,buffer,BUF_SIZE-1,0);
        printf("got %d bytes of normal data '%s'\n",ret,buffer);

        memset(buffer,'\0',BUF_SIZE);
        ret=recv(connection_fd,buffer,BUF_SIZE-1,MSG_OOB);//接收紧急数据
        printf("got %d bytes of oob data '%s'\n",ret,buffer);

        memset(buffer,'\0',BUF_SIZE);
        ret=recv(connection_fd,buffer,BUF_SIZE-1,0);
        printf("got %d bytes of normal data '%s'\n",ret,buffer);
        close(connection_fd);
    }
    //关闭监听描述符
    close(listenfd);
    return 0;
}