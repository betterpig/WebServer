#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <list>
#include <cstdio>
#include <pthread.h>
#include "locker.h"
#include "connection_pool.h"
#include "log.h"

template<typename T>//模板参数T是任务类
class ThreadPool
{
private:
    int m_thread_number;//线程池中的线程数量
    int m_max_requests;//请求队列中的最大请求数
    pthread_t* m_threads;//线程池id数组
    std::queue<T*> m_work_queue;//请求队列，用链表表示队列
    Locker m_queue_locker;//保护请求队列的互斥锁
    Sem m_queue_stat;//表明是否有任务需要处理的信号量
    bool m_stop;//线程结束的标志
    connection_pool* m_connpool;
    static void* Worker(void* arg);
    void Run();

public:
    ThreadPool(connection_pool* connpool,int thread_number=8,int max_requests=10000);
    ~ThreadPool();
    bool Append(T* request);
};

template<typename T>
ThreadPool<T>::ThreadPool(connection_pool* connpool,int thread_number,int max_requests)
:m_thread_number(thread_number),m_max_requests(max_requests),m_stop(false),m_threads(nullptr),m_connpool(connpool)
{
    if((thread_number<=0) || (max_requests<=0))
        throw std::exception();
    m_threads=new pthread_t[m_thread_number];
    if(!m_threads)
        throw std::exception();
    for(int i=0;i<thread_number;++i)
    {
        printf("create the %dth thread\n",i);
        //在c++中，pthread_create函数中第三个参数必须指向静态函数，而类的静态函数不包含this指针，也就无法直接调用真正的线程执行函数run
        //所以得增加worker函数作为入口，并将this指针作为参数传入，再在worker中通过this指针调用run函数
        if(pthread_create(m_threads+i,nullptr,Worker,this)!=0)//创建线程，把线程id存放在m_threads数组中的第i个位置，该线程运行的函数是WOrker，参数是this
        {
            LOG_ERROR("create the %dth thread failed",i);
            delete []m_threads;//创建线程失败，则删除整个线程数组并抛出异常
        }
        if(pthread_detach(m_threads[i]))//将线程设置为脱离线程：在退出时将自行释放其占有的系统资源
        {
            LOG_ERROR("detach the %dth thread failed",i);
            delete []m_threads;//设置失败则删除整个线程组并抛出异常
        }
    }
}

template<typename T>
ThreadPool<T>::~ThreadPool()
{
    delete []m_threads;//只是删除线程数组就好了。因为线程自己会退出
    m_stop=true;
}

template<typename T>
bool ThreadPool<T>::Append(T* request)
{
    m_queue_locker.Lock();//上锁，保证同一时间只有一个线程操作请求队列

    if(m_work_queue.size()>m_max_requests)//请求队列满，则解锁，返回错误
    {
        m_queue_locker.Unlock();
        return false;
    }
    m_work_queue.push(request);//将请求放到队列最后
    m_queue_stat.Post();//将信号量+1
    m_queue_locker.Unlock();//解锁
    return true;
}

template<typename T>
void* ThreadPool<T>::Worker(void* arg)
{
    ThreadPool* pool=(ThreadPool*) arg;
    pool->Run();
    return pool;
}

template<typename T>
void ThreadPool<T>::Run()
{
    while(!m_stop)
    {
        m_queue_stat.Wait();//阻塞，等待有请求来到.若信号量大于1，则执行-1操作
        m_queue_locker.Lock();//请求来到，给请求队列上锁
        if(m_work_queue.empty())//如果请求队列空了，说明请求被别的线程取走了
        {
            m_queue_locker.Unlock();//解锁
            continue;//重头开始等
        }
        T* request=m_work_queue.front();//取出队列头
        m_work_queue.pop();//将队列头popk
        m_queue_locker.Unlock();//解锁
        if(!request)//请求为空
            continue;
        connectionRAII mysqlcon(&request->mysql, m_connpool);
        request->Process();//需要保证类型T有process函数
    }   
}

#endif