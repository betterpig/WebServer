#ifndef TIMER_H
#define TIMER_H

#include "timer_base.h"
#include "time_list.h"
#include "time_heap.h"
#include "time_wheel.h"

enum timer_type {SORTLIST=0,HEAP,WHEEL};

class TimerContainer
{
public:
    TimerContainer(timer_type type=SORTLIST)
    {
        timer_ptr=nullptr;
        switch (type)
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
    }

    void* AddTimer(HttpConn* hc,unsigned int delay) {return timer_ptr->AddTimer(hc,delay);}
    void DeleteTimer(void* timer) {timer_ptr->DeleteTimer(timer);}
    void AdjustTimer(void* timer,unsigned int delay) {timer_ptr->AdjustTimer(timer,delay);}
    void Tick() {timer_ptr->Tick();}
private:
    TimerContainerBase* timer_ptr;
};


#endif