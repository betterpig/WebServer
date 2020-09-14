
template<typename T>
class Heap
{
public:
    Heap():capacity(50),size(0),heap(new T[capacity]){}
    ~Heap() { delete []heap; }
    bool BuildHeap(T arr[],int length)
    {
        if(length>capacity)
            resize(length*2);
        for(int i=0;i<length;i++)
            heap[i]=arr[i];
        size=length;
        for(int i=size/2;i>=0;i--)
        {
            adjust(i);
        }
    }

    T top() 
    {
        if(empty())
            return NULL;
        else
            return heap[0];
    }
    void pop()
    {
        if(empty())
            return;
        else
        {
            T temp=heap[0];
            delete temp;
            heap[0]=heap[size-1];
            heap[size-1]=NULL;
            size--;
            adjust(0);
        }
    }
    bool push(T item)
    {
        if(full())
            resize();
        
        heap[size++]=item;
        int child=size-1;
        for(int parent=(child-1)/2;child>0;parent=(child-1)/2)
        {
            if(heap[parent]<item)
                break;
            else
            {
                heap[child]=heap[parent];
                child=parent;
            }
        }
        heap[child]=item;
        return true;
    }
    
private:
    int capacity;
    int size;
    T* heap;
    bool resize() {resize(2*capacity);}
    bool resize(int new_capacity)
    {
        if(new_capacity<capacity)
            return false;
        
        T* temp=new T[new_capacity];
        for(int i=0;i<size;i++)
        {
            temp[i]=heap[i];
        }
        delete []heap;
        heap=temp;
        return true;
    }
    bool adjust(int parent)
    {
        if(parent<0 || parent>=size-1)
            return false;
        
        int child=0;
        int temp=heap[parent];
        for(child=parent*2+1;child<size;child=parent*2+1)
        {
            if(child+1<size && heap[child+1]<heap[child])
                child++;
            if(heap[child]>=temp)
                break;
            else
            {
                heap[parent]=heap[child];
                parent=child;
            }
        }
        heap[parent]=temp;
        return true;
    }
    bool empty() { return size==0; }
    bool full() { return size==capacity; }
};