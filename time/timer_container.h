#ifndef TIMER_H//定时器容器接口类
#define TIMER_H

#include "timer_base.h"
#include "time_list.h"
#include "time_heap.h"
#include "time_wheel.h"
#include "../log.h"

enum timer_type {SORTLIST=0,HEAP,WHEEL};//定时器容器类型
char* type_to_string[]={"SORTLIST","HEAP","WHEEL"};
class TimerContainer//定时器容器接口类
{
public:
    TimerContainer(timer_type type=SORTLIST)//构造函数
    {
        timer_ptr=nullptr;
        switch (type)//根据类型创建不同的定时器容器
        {
        case SORTLIST:
            timer_ptr=new SortTimerList;
            break;
        case HEAP:
            timer_ptr=new TimeHeap;
            break;
        case WHEEL:
            timer_ptr=new TimeWheel;
            break;
        default:
            break;
        }
        if(timer_ptr==nullptr)
        {
            LOG_ERROR("create timer_container of %s failed",type_to_string[type]);
            exit(EXIT_FAILURE);
        }
    }
    ~TimerContainer()
    {
        delete timer_ptr;
    }
    //通过基类指针，在运行时调用实际类型的对应函数
    void* AddTimer(HttpConn* hc,unsigned int delay) {return timer_ptr->AddTimer(hc,delay);}
    void DeleteTimer(void* timer) {timer_ptr->DeleteTimer(timer);}
    void AdjustTimer(void* timer,unsigned int delay) {timer_ptr->AdjustTimer(timer,delay);}
    void Tick() {timer_ptr->Tick();}
private:
    TimerContainerBase* timer_ptr;
};


#endif