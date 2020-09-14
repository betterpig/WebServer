#include "http_conn.h"

#include "connection_pool.h"
#include "time/timer_container.h"
#include "block_queue.h"
#include "common.h"

class ThreadPool;
class Server
{
friend void SigHandler(int sig);
public:
    static Server* GetServer();
    int Init(int argc,char** args);
    void EventLoop();
    void Modfd(int fd,int ev);
    void Removefd(int fd);

private:
    ConnectionPool* connpool;
    ThreadPool* thread_pool;
    TimerContainer* timer_container;
    HttpConn* users;
    BlockQueue<HttpConn*>* requet_queue;
    
    int listenfd;
    int sigpipe[2];
    int epollfd;

    Server() { }
    ~Server();
    int Configure();
    void StartListen(int argc,char** args);
    void AcceptCallback();
    void SignalCallback(bool& stop_server,bool& time_out);
    void CloseCallback(int sockfd);
    void ReadCallback(int sockfd);
    void WriteCallback(int sockfd);
    void ShowError(int connfd,const char* info);

    void AddSig(int sig,void (*handler) (int),bool restart=true);
    int SetNonBlocking(int fd);
    void Addfd(int fd,bool oneshot);
    
    
};  