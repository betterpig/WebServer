#ifndef TIME_WHELL_TIMER//时间轮
#define TIME_WHELL_TIMER

#include "timer_base.h"

struct WheelTimer//时间轮定时器容器的定时器结构体
{
    HttpConn* user_data;
    int rotation;//记录定时器在转多少圈后生效
    int time_slot;//记录定时器属于哪个槽
    WheelTimer* next;
    WheelTimer* prev;

    WheelTimer(HttpConn* hc,int rot,int ts):user_data(hc),rotation(rot),time_slot(ts),next(nullptr),prev(nullptr) {}
};

class TimeWheel:public TimerContainerBase
{
private:
    static const int N=60;//槽数
    static const int SI=TIMESLOT;//槽间隔
    WheelTimer* slots[N];//链表数组
    int cur_slot;//当前所在的槽
public:
    TimeWheel():cur_slot(0)
    {
        for(int i=0;i<N;++i)
            slots[i]=NULL;//初始化每个槽的头节点
    }
    virtual ~TimeWheel() override
    {
        for(int i=0;i<N;++i)//遍历每个槽
        {
            WheelTimer* tmp=slots[i];
            while(tmp)//把该槽的链表上的每个节点都释放内存
            {
                slots[i]=tmp->next;
                delete tmp;
                tmp=slots[i];
            }
        }
    }

    virtual void* AddTimer(HttpConn* hc,unsigned int delay) override//根据定时值创建定时器，并插到合适的槽中
    {
        int ticks=delay/SI;
        int rotation=ticks/N;//根据槽间隔算需要转多少圈才触发该定时器
        int ts=(cur_slot+(ticks%N))%N;//计算该定时器应该放在哪个槽：当前槽+增量槽再对总槽数取摸
        WheelTimer* timer=new WheelTimer(hc,rotation,ts);//新建定时器，给定圈数和槽位

        if(!slots[ts])//如果要插入的槽为空，那把该定时器作为头节点
            slots[ts]=timer;
        else//否则 把定时器插到最前面：头插法 ？为什么不像升序链表那样按时间先后插入
        {
            timer->next=slots[ts];
            slots[ts]->prev=timer;
            slots[ts]=timer;
        }
        return timer;
    }
    
    virtual void AdjustTimer(void* timer_,unsigned int delay) override
    {
        if(!timer_)
            return;
        WheelTimer* timer=(WheelTimer*) timer_;
        int ts=timer->time_slot;
        if(timer==slots[ts])//如果要删除的是头节点
        {
            slots[ts]=slots[ts]->next;//下一个节点变成头节点
            if(slots[ts])
                slots[ts]->prev=nullptr;//新头节点的前驱节点置空
        }
        else
        {
            if(timer->prev)
                timer->prev->next=timer->next;//将要删除节点的前驱节点的next指向要删除节点的后继节点
            if(timer->next)
                timer->next->prev=timer->prev;//将要删除节点的后继节点的prev指向要删除节点的前驱节点
        }

        int ticks=delay/SI;
        timer->rotation+=ticks/N;
        timer->time_slot=(timer->time_slot+ticks)%N;
        ts=timer->time_slot;
        if(!slots[ts])//如果要插入的槽为空，那把该定时器作为头节点
            slots[ts]=timer;
        else//否则 把定时器插到最前面：头插法 ？为什么不像升序链表那样按时间前后插入
        {
            timer->next=slots[ts];
            slots[ts]->prev=timer;
            slots[ts]=timer;
        }
    }

    virtual void DeleteTimer(void* timer_) override
    {
        if(!timer_)
            return;
        WheelTimer* timer=(WheelTimer*) timer_;
        int ts=timer->time_slot;
        if(timer==slots[ts])//如果要删除的是头节点
        {
            slots[ts]=slots[ts]->next;//下一个节点变成头节点
            if(slots[ts])
                slots[ts]->prev=nullptr;//新头节点的前驱节点置空
            delete timer;
        }
        else
        {
            if(timer->prev)
                timer->prev->next=timer->next;//将要删除节点的前驱节点的next指向要删除节点的后继节点
            if(timer->next)
                timer->next->prev=timer->prev;//将要删除节点的后继节点的prev指向要删除节点的前驱节点
            delete timer;
        }
    }

    virtual void Tick() override
    {
        WheelTimer* tmp=slots[cur_slot];//当前槽的链表的头节点
        while(tmp)
        {
            if(tmp->rotation>0)//圈数大于0说明当前时刻还没到时间，只有当圈数等于0，才到期了
            {
                tmp->rotation--;//但需要将圈数减一
                tmp=tmp->next;
            }
            else
            {
                tmp->user_data->CloseConn();//到期则调用回调函数
                if(tmp==slots[cur_slot])//如果到期的是头节点
                {
                    slots[cur_slot]=tmp->next;//将新头节点设为原头节点的后继节点
                    delete tmp;
                    if(slots[cur_slot])//如果新头节点不为空
                        slots[cur_slot]->prev=nullptr;
                    tmp=slots[cur_slot];
                }
                else//类似于删除节点的操作
                {
                    tmp->prev->next=tmp->next;
                    if(tmp->next)
                        tmp->next->prev=tmp->prev;
                    WheelTimer* tmp2=tmp->next;
                    delete tmp;
                    tmp=tmp2;
                }
            }
        }
        cur_slot=(++cur_slot)%2;//当前槽运行到下一个槽
        alarm(TIMESLOT);
    }
};

#endif