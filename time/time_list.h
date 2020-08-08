#ifndef TIME_LIST_H//基于升序链表的定时器容器
#define TIME_LIST_H

#include "timer_base.h"

struct ListTimer//基于升序链表的定时器容器的定时器结构体
{
    HttpConn* user_data;//客户连接类指针
    time_t expire;//任务的超时时间，绝对时间，即在什么时候终止
    ListTimer* prev;//指向前一个节点
    ListTimer* next;//指向后一个节点

    ListTimer(HttpConn* hc,int set_time):user_data(hc),expire(set_time),prev(nullptr),next(nullptr) {}
};

class SortTimerList : public TimerContainerBase//定时器链表，升序，双向链表，带有头节点和尾节点
{
private:
    ListTimer* head;//头节点
    ListTimer* tail;//尾节点

public:
    SortTimerList():head(nullptr),tail(nullptr) {}//构造函数
    virtual ~SortTimerList () override//析构函数，释放每个节点的内存
    {
        ListTimer* tmp=head;
        while(tmp)
        {
            head=tmp->next;
            delete tmp;
            tmp=head;
        }
    }
    virtual void* AddTimer(HttpConn* hc,unsigned int delay) override//添加一个定时器节点，参数是节点指针
    {
        int set_time=time(nullptr)+delay;//设置闹钟，该定时器将在3*TIMESLOT后到期，届时将关闭该连接
        ListTimer*timer=new ListTimer(hc,set_time);//将定时器与该连接绑定
        if(!head)//链表为空，直接令head和tail等于传入节点指针就好了
        {
            head=tail=timer;
            return timer;
        }
        if(timer->expire<head->expire)//当插入定时器的终止时间小于头节点的终止时间时，要插在头节点之前
        {
            timer->next=head;
            head->prev=timer;
            head=timer;//现在新插入的节点是头节点了
            return timer;
        }
        AddTimer(timer,head);//其他情况，调用重载函数，找到合适的位置插入，保证升序
    }

    virtual void AdjustTimer(void* timer_,unsigned int delay) override//定时器的终止时间改变，需要调整在链表中的位置，只考虑终止时间变长的情况，需要往链表尾部移动
    {
        if(!timer_)
            return;
        ListTimer* timer=(ListTimer*) timer_;
        timer->expire=time(nullptr)+delay;
        ListTimer* tmp=timer->next;
        if(!tmp || (timer->expire < tmp->expire))//需要改变的定时器节点已经是最后一个节点，或者定时器的终止时间仍然小于它后一个节点，则不需要改变位置
            return;
        if(timer==head)//需要改变的定时器节点是头节点，且它的新的终止时间大于后一个节点
        {//那需要改变的定时器节点要往后移动，它后一个节点变成新的头节点，问题变成在头节点之后找一个位置插入
            head=head->next;
            head->prev=nullptr;
            timer->next=nullptr;
            AddTimer(timer,head);
        }
        else//需要改变的定时器节点在头节点之后，先把它从原来的地方抹去，再在它后一个节点到尾节点这一段中找个位置插入
        {
            timer->prev->next=timer->next;//跳过该节点
            timer->next->prev=timer->prev;
            AddTimer(timer,timer->next);
        }
    }

    virtual void DeleteTimer(void* timer_) override//当某个定时器到点了，完成了回调函数，就把它从链表中删除
    {
        if(!timer_)
            return;
        ListTimer* timer=(ListTimer*) timer_;
        if((timer==head) && (timer==tail))//链表只有一个节点
        {
            delete timer;
            head=nullptr;
            tail=nullptr;
            return;
        }

        if(timer==head)//要删除的是头节点
        {
            head=head->next;
            head->prev=nullptr;
            delete timer;
            return ;
        }
        if(timer==tail)//要删除的是尾节点
        {
            tail=tail->prev;//前一个节点变成尾节点
            tail->next=nullptr;
            delete timer;
            return;
        }

        timer->prev->next=timer->next;//要删除的是中间节点，跳过该节点然后删除
        timer->next->prev=timer->prev;
        delete timer;
    }

    virtual void Tick() override//SIGALRM信号每次触发就会子啊信号处理函数中执行一次tick函数，处理链表上到期的任务
    {
        if(!head)//链表为空
        {
            alarm(TIMESLOT);
            return;
        }
        time_t cur=time(nullptr);//获得当前时间
        ListTimer* tmp=head;//从头节点开始检查
        while(tmp)//因为是升序排列，所以所有到期的定时器都是按它们原本应该的顺序执行其回调函数
        {
            if(cur<tmp->expire)//如果当前节点的终止时间大于当前系统时间，说明该节点往后的定时器节点都没到期，因为升序！
                break;
            tmp->user_data->CloseConn();//对到期的定时器，执行它自己的函数成员：回调函数，传入的参数是它自己的参数成员：客户端数据
            head=tmp->next;
            if(head)
                head->prev=nullptr;//善后
            delete tmp;
            tmp=head;
        }
        alarm(TIMESLOT);
    }

private:
    void* AddTimer(ListTimer* timer,ListTimer* lst_head)//对于插入节点在链表中间的
    {
        ListTimer* prev=lst_head;//插入节点保证在lst_head后面
        ListTimer* tmp=prev->next;
        while(tmp)
        {
            if(timer->expire<tmp->expire)//把要插入的节点插入到第一个终止时间比它大的节点之前
            {
                prev->next=timer;
                timer->next=tmp;
                tmp->prev=timer;
                timer->prev=prev;
                break;
            }
            prev=tmp;
            tmp=tmp->next;
        }
        if(!tmp)//遍历完还没找到终止时间比要插入节点大的，就把该节点插到最后
        {
            prev->next=timer;
            timer->prev=prev;
            timer->next=nullptr;
            tail=timer;//更新尾节点
        }
        return timer;
    }
};


#endif