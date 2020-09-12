#include "http_conn.h"
#include "thread_pool.h"
#include "connection_pool.h"
#include "time/timer_container.h"
#include "block_queue.h"

enum ActMode{REACTOR=0,PROACTOR};

class Server
{
public:
    static Server* GetServer();
    int Init(int argc,char** args);
    void EventLoop();


private:
    TimerContainer* timer_container;
    HttpConn* users;
    BlockQueue<HttpConn*>* requet_queue;
    
    int listenfd;

    Server() { }
    ~Server();
    int Configure();
    void StartListen();
};