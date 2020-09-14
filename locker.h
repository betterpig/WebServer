#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

class Sem//信号量
{
private:
    sem_t m_sem;
public:
    Sem(unsigned int init_value=0)
    {
        if(sem_init(&m_sem,0,init_value) !=0 )
            throw std::exception();//构造函数没有返回值，需要通过抛出异常来报告错误
    }
    ~Sem()
    {
        sem_destroy(&m_sem);
    }
    bool wait() {return sem_wait(&m_sem) ==0; }
    bool post() {return sem_post(&m_sem)==0; }
};

class MutexLock//互斥锁
{
private:
    pthread_mutex_t mutex_;
public:
    MutexLock()
    {
        if(pthread_mutex_init(&mutex_,nullptr) !=0 )
            throw std::exception();
    }
    ~MutexLock()
    {
        pthread_mutex_destroy(&mutex_);
    }
    pthread_mutex_t* GetPthreadMutex() { return &mutex_; }
    bool lock() { return pthread_mutex_lock(&mutex_) ==0; }
    bool unlock()   { return pthread_mutex_unlock(&mutex_) == 0; }
};

class MutexLockGuard
{
public:
    explicit MutexLockGuard(MutexLock& mutex):mutex_(mutex)
    {
        mutex_.lock();
    }
    ~MutexLockGuard()
    {
        mutex_.unlock();
    }
private:
    MutexLock& mutex_;
};


class ConditionalPara//条件变量
{
private:
    MutexLock& mutex_;//条件变量需要用到锁
    pthread_cond_t cond_;

public:
    ConditionalPara(MutexLock& mutex):mutex_(mutex)
    {
        pthread_cond_init(&cond_,nullptr);//初始化条件变量
    }
    ~ConditionalPara()
    {
        pthread_cond_destroy(&cond_);
    }

    bool wait()
    {
        int ret=pthread_cond_wait(&cond_,mutex_.GetPthreadMutex());//在调用该函数之前，就要对锁加锁
        return ret==0;
    }
    bool notify()   {return pthread_cond_signal(&cond_)==0;}
    bool NotifyAll()    {return pthread_cond_broadcast(&cond_)==0;}
};

#endif