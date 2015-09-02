#pragma once
#include "Require.h"

TINYNET_START()
    
class Scheduler
{
public:
    static Scheduler& Instance()
    {
        static Scheduler instance;
        return instance;
    }

    Scheduler() : _running(0), _thread(NULL), _timerNext(0)
    {
    }

    void Start();

    void Close();

    /// first time in delay ms
    /// period = 0, run once
    /// period > 0, run periodically with period ms interval
    uint32_t Schedule(const std::function<void()>& func, uint32_t delay, uint32_t period = 0);
    
    template<class T>
    uint32_t Schedule(const T& func, uint32_t delay, uint32_t period = 0)
    {
        return Schedule(std::function<void()>(func), delay, period);
    }

    /// if it's running, it'll wait until runs over 
    void ShutDown(uint32_t name);
private:
    static DWORD WINAPI ThreadProc(LPVOID);

    void MainLoop();

    HANDLE      _thread;
    uint32_t    _running;
private:
    struct Timer
    {
        uint32_t    _name;

        uint32_t    _dueTime;
        uint32_t    _period;

        volatile bool    _running;
        volatile bool    _retired;
        
        std::function<void()>    _action;
    };

    typedef std::shared_ptr<Timer> TimerPtr;

    struct TimerComparer
    {
        bool operator()(const TimerPtr& lhs, const TimerPtr& rhs)
        {
            return lhs->_dueTime > rhs->_dueTime;
        }
    };

    typedef std::unique_lock<std::mutex> LockGuard;

    typedef std::map<uint32_t, TimerPtr> TimerMap;
    
    typedef std::priority_queue<TimerPtr, std::vector<TimerPtr>, TimerComparer> TimerQueue;

    std::mutex    _timerLock;
    TimerMap      _timerMap;
    TimerQueue    _timerQueue;

    volatile uint32_t    _timerNext;
    
    std::condition_variable    _condition;
};

#define theScheduler Scheduler::Instance()

TINYNET_CLOSE()
