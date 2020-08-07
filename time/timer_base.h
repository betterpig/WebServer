#ifndef TIMER_BASE_H//定时器容器抽象基类
#define TIMER_BASE_H

#include <time.h>
#include <unistd.h>
#include "../http_conn.h"

#define TIMESLOT 5

class TimerContainer;//定时器容器借口类
class TimerContainerBase//定时器容器抽象基类：统一函数接口
{
friend class TimerContainer;
public:
    TimerContainerBase(){}
    //virtual ~TimerBase()=0;
    virtual void* AddTimer(HttpConn* hc,unsigned int delay)=0;//添加定时器
    virtual void AdjustTimer(void* timer_,unsigned int delay)=0;//调整（延后）定时器
    virtual void DeleteTimer(void* timer_)=0;//删除定时器
    virtual void Tick()=0;//滴答函数，处理到期定时器
};

#endif