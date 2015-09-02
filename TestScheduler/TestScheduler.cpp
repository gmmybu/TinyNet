#include "Scheduler.h"
using namespace TinyNet;

#pragma comment(lib, "TinyNet.lib")

int main()
{
    theScheduler.Start();

    uint32_t timer1 = theScheduler.Schedule([=] {
        printf("run once, %d\n", GetTickCount());
    }, 1000);

    static int index = 0;
    uint32_t timer2 = theScheduler.Schedule([=] {
        //++index;
        printf("timer2 %d %d\n", GetTickCount(), ++index);
    }, 1000, 2000);

    
    static int index2 = 0;
    uint32_t timer3 = theScheduler.Schedule([=] {
        ++index2;
    }, 176, 100);

    ::Sleep(10000);
    theScheduler.ShutDown(timer3);

    printf("%d", index2);

    ::Sleep(200000);
    theScheduler.ShutDown(timer2);

    printf("%d", index);

    getchar();

    theScheduler.Close();

    return 0;
}