#include "timer_container.h"

int main()
{
    TimerContainer tc(SORTLIST);
    ListTimer* t=new ListTimer;
    tc.AddTimer(t);
    return 0;
}