#include "server.h"

int main(int argc,char** argv)
{
    if(argc<=2)
    {
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
        return 1;
    }
    Server* server=Server::GetServer();
    server->Init(argc,argv);
    server->EventLoop();
}