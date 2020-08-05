#ifndef TIMER_BASE_H
#define TIMER_BASE_H

#include <time.h>
#include "http_conn.h"

class HttpConn;
struct ListTimer//相当于链表中的节点结构体，它自己既保存了回调函数，又保存了用于回调函数的参数 userdata
{
public:
    HttpConn* user_data;//在类UtilTimer中，又要用到client_data结构体来定义指针，二者互相使用
    time_t expire;//任务的超时时间，绝对时间，即在什么时候终止
    //回调函数是外部定义的，然后作为节点的成员
    ListTimer* prev;//指向前一个节点
    ListTimer* next;//指向后一个节点

public:
    ListTimer(HttpConn* hc,int set_time):user_data(hc),expire(set_time),prev(nullptr),next(nullptr) {}
};

struct HeapTimer
{
public:
    time_t expire;
    HttpConn* user_data;
    HeapTimer(int delay)
    {
        expire=time(nullptr) + delay;
    }
};

struct WheelTimer
{
public:
    int rotation;
    int time_slot;
    WheelTimer* next;
    WheelTimer* prev;
    HttpConn* user_data;

    WheelTimer(int rot,int ts):next(nullptr),prev(nullptr),rotation(rot),time_slot(ts){}
};


class TimerContainer;
class TimerBase
{
friend class TimerContainer;
public:
    TimerBase(){}
    //virtual ~TimerBase()=0;
    virtual void* AddTimer(HttpConn* hc)=0;
    virtual void AdjustTimer(void* timer_)=0;
    virtual void DeleteTimer(void* timer_)=0;
    virtual void Tick()=0;
};

#endif