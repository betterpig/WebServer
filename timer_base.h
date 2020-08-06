#ifndef TIMER_BASE_H
#define TIMER_BASE_H

#include <time.h>
#include <unistd.h>
#include "http_conn.h"

#define TIMESLOT 5

struct ListTimer//相当于链表中的节点结构体，它自己既保存了回调函数，又保存了用于回调函数的参数 userdata
{
    HttpConn* user_data;//在类UtilTimer中，又要用到client_data结构体来定义指针，二者互相使用
    time_t expire;//任务的超时时间，绝对时间，即在什么时候终止
    //回调函数是外部定义的，然后作为节点的成员
    ListTimer* prev;//指向前一个节点
    ListTimer* next;//指向后一个节点

    ListTimer(HttpConn* hc,int set_time):user_data(hc),expire(set_time),prev(nullptr),next(nullptr) {}
};

struct HeapTimer
{
    HttpConn* user_data;
    time_t expire;
    int index;
    HeapTimer(HttpConn* hc,int set_time):user_data(hc),expire(set_time),index(0) {}
};

struct WheelTimer
{
    int rotation;//记录定时器在转多少圈后生效
    int time_slot;//记录定时器属于哪个槽
    WheelTimer* next;
    WheelTimer* prev;
    HttpConn* user_data;

    WheelTimer(int rot,int ts):rotation(rot),time_slot(ts),next(nullptr),prev(nullptr) {}
};


class TimerContainer;
class TimerContainerBase
{
friend class TimerContainer;
public:
    TimerContainerBase(){}
    //virtual ~TimerBase()=0;
    virtual void* AddTimer(HttpConn* hc,unsigned int delay)=0;
    virtual void AdjustTimer(void* timer_,unsigned int delay)=0;
    virtual void DeleteTimer(void* timer_)=0;
    virtual void Tick()=0;
};

#endif