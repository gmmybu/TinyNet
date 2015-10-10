#include "Scheduler.h"


TINYNET_START()

void Scheduler::Start()
{
    if (InterlockedCompareExchange(&_running, 1, 0) == 0) {
        _thread = CreateThread(NULL, 0, &Scheduler::ThreadProc, NULL, 0, NULL);
        if (_thread == NULL)
            throw std::exception("Scheduler::Start, 1");
    }
}

void Scheduler::Close()
{
    if (InterlockedCompareExchange(&_running, 0, 1) == 1 && _thread != NULL) {
        {
            LockGuard guard(_timerLock);
            _condition.notify_one();
        }

        WaitForSingleObject(_thread, INFINITE);
        CloseHandle(_thread);
        _thread = NULL;

        {
            LockGuard guard(_timerLock);
            _timerQueue = TimerQueue();
            _timerMap.clear();
        }
    }
}

DWORD Scheduler::ThreadProc(LPVOID)
{
    theScheduler.MainLoop();
    return 0;
}

void Scheduler::MainLoop()
{
    while (true) {
        /// try to acquire next timer
        TimerPtr timer;
        while (_running) {
            LockGuard guard(_timerLock);

            uint32_t curTime = GetTickCount();
            uint32_t dueTime = curTime + 1000;
            if (_timerQueue.size() > 0) {
                /// erase retired timer
                TimerPtr topper = _timerQueue.top();
                if (topper->_retired) {
                    _timerQueue.pop();
                    continue;
                }

                dueTime = topper->_dueTime;
                if (dueTime <= curTime) {
                    timer = topper;
                    timer->_running = true;
                    _timerQueue.pop();
                    break;
                }
            }

            std::chrono::duration<int, std::milli> elapsed(dueTime - curTime);
            _condition.wait_for(guard, elapsed);
        }

        if (!_running) break;

        try
        {
            /// run timer
            timer->_action();
        }
        catch(...)
        {
        }

        timer->_running = false;

        LockGuard guard(_timerLock);
        if (timer->_period == 0) {
            _timerMap.erase(timer->_name);
        } else {
            /// retired timer will be erased later
            timer->_dueTime += timer->_period;
            _timerQueue.push(timer);
        }
    }
}

uint32_t Scheduler::Schedule(const std::function<void()>& func, uint32_t delay, uint32_t period)
{
    TimerPtr timer(new Timer);
    timer->_action  = func;
    timer->_period  = period;
    timer->_running = false;
    timer->_retired = false;
    timer->_dueTime = GetTickCount() + delay;

    uint32_t name = InterlockedIncrement(&_timerNext);
    timer->_name = name;

    LockGuard guard(_timerLock);
    _timerMap.emplace(name, timer);
    _timerQueue.push(timer);

    /// always wake up mainloop in case timer will run imediately
    _condition.notify_one();
    return name;
}

void Scheduler::ShutDown(uint32_t name)
{
    LockGuard guard(_timerLock);

    auto iter = _timerMap.find(name);
    if (iter != _timerMap.end()) {
        iter->second->_retired = true;
        
        /// should use spin-wait
        while (iter->second->_running) {
            ::Sleep(1);
        }
    }
}

TINYNET_CLOSE()