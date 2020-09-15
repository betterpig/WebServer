#include "server.h"
#include "log.h"

#define ASYNLOG

int main(int argc,char** argv)
{
    /*if(daemon(1,0) == -1)
    {
        perror("daemon error");
        exit(EXIT_FAILURE);
    }*/
    if(argc<=2)
    {
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
        return 1;
    }
    #ifdef ASYNLOG
        Log::GetInstance()->init("ServerLog", 2000, 8000, 8); //异步日志模型
    #else
        Log::GetInstance()->init("ServerLog", 2000, 8000, 0); //同步日志模型
    #endif

    for(int i=0;i<10;i++)
        LOG_INFO("LALALA %d",i);
    Server* server=Server::GetServer();
    server->Init(argc,argv);
    server->EventLoop();
    
}