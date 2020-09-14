#ifndef TIME_HEAP//时间堆
#define TIME_HEAP

#include "timer_base.h"

struct HeapTimer//基于最小堆的定时器容器的定时器结构体
{
    HttpConn* user_data;
    time_t expire;
    int index;//在最小堆数组中的索引
    HeapTimer(HttpConn* hc,int set_time):user_data(hc),expire(set_time),index(0) {}
};

class TimeHeap:public TimerContainerBase
{
private:
    HeapTimer* (*array);//定时器结构体指针数组
    int capacity;//容量
    int cur_size;//当前元素个数

public:
    TimeHeap():capacity(50),cur_size(0)
    {
        array=new HeapTimer*[capacity];//定时器结构体指针数组
        for(int i=0;i<capacity;++i)
            array[i]=nullptr;//每个元素都置为空指针
    }

    virtual ~TimeHeap() override
    {
        for(int i=0;i<cur_size;++i)
            delete array[i];//释放每个指针指向的定时器结构体
        delete []array;//释放指针数组这个动态分配内存
    }

    virtual void* AddTimer(HttpConn* hc,unsigned int delay) override
    {
        int set_time=time(nullptr)+delay;
        HeapTimer* timer=new HeapTimer(hc,set_time);
        if(cur_size>=capacity)
            Resize();//数组满了则分配更大的内存
        int child=cur_size++;//新定时器指针初始插入位置
        int parent=0;
        for(;child>0;child=parent)//上浮
        {
            parent = (child-1)/2;//定时器父节点
            if(array[parent]->expire <= timer->expire)//直到给插入节点找到合适的位置，类似插入排序
                break;
            else
                array[child] = array[parent];//如果父节点的终止时间大于插入定时器的终止时间，则把父节点拉下来，
        }
        array[child]=timer;//把插入节点放在break的位置
        timer->index=child;
        return timer;
    }

    virtual void AdjustTimer(void* timer_,unsigned int delay)
    {
        if(!timer_)
            return;
        HeapTimer* timer=(HeapTimer*) timer_;
        timer->expire=time(nullptr)+delay;
        PercolateDown(timer->index);
    }

    virtual void DeleteTimer(void* timer_) override
    {//最小堆只能删除最顶端数据
        if(!timer_)
            return;
        HeapTimer* timer=(HeapTimer*) timer_;
        if(timer!=array[cur_size-1])//这里有问题，当前面的定时器删除后，后面的定时器的索引都应该更新，这里没执行这一步
        {
            int temp_index = timer->index;
            array[timer->index] = array[cur_size-1];
            array[cur_size-1]=nullptr;
            cur_size--;
            delete timer;
            PercolateDown(temp_index);
        }
        else
        {
            delete timer;
            array[cur_size-1]=nullptr;
            cur_size--;
        }
    }

    virtual void Tick()
    {
        time_t cur=time(nullptr);//获取当前时间
        while(!empty())//堆不空时
        {
            if(!array[0])
                break;
            if(array[0]->expire > cur)
                break;
            //终止时间小于当前时间，就要先关闭连接，然后抛出该元素
            array[0]->user_data->CloseConn();
            Pop();
        }
        if(empty())
            alarm(TIMESLOT);
        else
        {
            int time_interval=top()->expire - cur;
            alarm(time_interval);
        }
    }

private:
    bool empty() const { return cur_size==0;}
    HeapTimer* top() const
    {
        if(empty())
            return nullptr;
        return array[0];//返回最顶端数据：最快到终止时间的
    }

    void Pop()
    {
        if(empty())
            return;
        if(array[0])
        {
            delete array[0];
            array[0]=array[cur_size];//先把数组最后一个元素放到堆顶
            array[cur_size--]=nullptr;//再将最后一个元素置为空指针
            if(cur_size>1)
                PercolateDown(0);//再对堆顶元素做下沉操作
        }
    }

    void PercolateDown(int hole)//下沉
    {
        HeapTimer* tmp=array[hole];//需要下沉的节点
        int child=0;
        for(;(hole*2+1)<=cur_size-1;hole=child)//终止条件（当前节点至少有左孩子）
        {
            child=hole*2+1;//获得它的左孩子
            if((child<cur_size-1) && (array[child+1]->expire<array[child]->expire))//如果右孩子存在且终止时间小于左孩子
                ++child;//那么取两个孩子中的较小的
            if(array[child]->expire<tmp->expire)//如果该较小的孩子小于父节点
                array[hole]=array[child];//把该较小的孩子上浮到父节点上，父节点下沉到该孩子节点（hole=child)
            else
                break;//否则需要下沉的节点就应该放在当前这个父节点的位置上
        }
        array[hole]=tmp;//把需要下沉节点放到最终的位置上
    }

    void Resize()
    {
        HeapTimer** tmp=new HeapTimer* [2*capacity];//分配当前容量两倍的动态内存
        for(int i=0;i<2*capacity;++i)
            tmp[i]=nullptr;
        if(!tmp)
            throw std::exception();
        capacity=2*capacity;
        for(int i=0;i<cur_size;++i)//把原数组里的每个元素拷贝过来
            tmp[i]=array[i];
        delete []array;//释放原数组的内存
        array=tmp;//将新数组的地址赋给array
    }
};

#endif