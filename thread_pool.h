#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "connection_pool.h"
#include "log.h"
#include "block_queue.h"
#include "locker.h"

class ThreadPool
{
private:
    int m_thread_number;//线程池中的线程数量
    int m_max_requests;//请求队列中的最大请求数
    pthread_t* m_threads;//线程池id数组
    BlockQueue<HttpConn*>* m_work_queue;//请求队列
    bool m_stop;//线程结束的标志
    ConnectionPool* m_connpool;
    static void* Worker(void* arg);
    void Run();

public:
    ThreadPool(ConnectionPool* connpool,BlockQueue<HttpConn*>* work_queue,int thread_number=8,int max_requests=10000);
    ~ThreadPool();
};

ThreadPool::ThreadPool(ConnectionPool* connpool,BlockQueue<HttpConn*> *work_queue,int thread_number,int max_requests)
:m_thread_number(thread_number),m_max_requests(max_requests),m_stop(false),m_threads(nullptr),m_connpool(connpool),m_work_queue(work_queue)
{
    if((thread_number<=0) || (max_requests<=0))
        throw std::exception();
    m_threads=new pthread_t[m_thread_number];
    if(!m_threads)
        throw std::exception();
    for(int i=0;i<thread_number;++i)
    {
        //在c++中，pthread_create函数中第三个参数必须指向静态函数，而类的静态函数不包含this指针，也就无法直接调用真正的线程执行函数run
        //所以得增加worker函数作为入口，并将this指针作为参数传入，再在worker中通过this指针调用run函数
        if(pthread_create(m_threads+i,nullptr,Worker,this)!=0)//创建线程，把线程id存放在m_threads数组中的第i个位置，该线程运行的函数是WOrker，参数是this
        {
            delete []m_threads;//创建线程失败，则删除整个线程数组并抛出异常
            perror("create thread failed");
        }
        
        /*if(pthread_detach(m_threads[i]))//将线程设置为脱离线程：在退出时将自行释放其占有的系统资源
        {
            delete []m_threads;//设置失败则删除整个线程组并抛出异常
            perror("detach thread failed");
        }*/
    }
}

ThreadPool::~ThreadPool()
{
    m_stop=true;
    m_work_queue->m_stop=true;
    m_work_queue->cond_.NotifyAll();
    for(int i=0;i<m_thread_number;i++)
    {
        if(pthread_join(m_threads[i],NULL)==0)
        {
            printf("work thread exit %d\n",m_threads[i]);
            LOG_INFO("work thread exit %d",m_threads[i]);
        }
        else
            perror("wait thread exit:");
    }
    delete []m_threads;//只是删除线程数组就好了。因为线程自己会退出
}

void* ThreadPool::Worker(void* arg)
{
    printf("work thread %d begin to work\n",pthread_self());
    LOG_INFO("work thread %d begin to work",pthread_self());
    ThreadPool* pool=(ThreadPool*) arg;
    pool->Run();
    pthread_exit(0);
}

void ThreadPool::Run()
{
    while(!m_stop)
    {
        HttpConn* request=nullptr;
        m_work_queue->pop(request);//取出队列头
        
        if(!request)//请求为空
            continue;
        connectionRAII mysqlcon(&request->mysql, m_connpool);
        request->Process();//需要保证类型T有process函数
    }  
}

#endif