#pragma once
#include <set>
#include <map>
#include <list>
#include <memory>
#include <string>
#include <vector>
#include <stdint.h>
#include <WinSock2.h>
#include <Mswsock.h>
#include <windows.h>
#include <type_traits>
#pragma comment(lib, "ws2_32.lib")

#define NOCOPYASSIGN(clazz)         \
    clazz(const clazz&);            \
    clazz& operator=(const clazz&); \
                                    \
    clazz(clazz&&);                 \
    clazz& operator=(clazz&&)

#define NAMESPACE_START(space) namespace space {
#define NAMESPACE_CLOSE(space) }

NAMESPACE_START(TinyNet)

class Mutex
{
    NOCOPYASSIGN(Mutex);
public:
    Mutex()
    {
        InitializeCriticalSectionAndSpinCount(&_mutex, 100);
    }

    ~Mutex()
    {
        DeleteCriticalSection(&_mutex);
    }

    void Enter()
    {
        EnterCriticalSection(&_mutex);
    }

    void Leave()
    {
        LeaveCriticalSection(&_mutex);
    }
private:
    CRITICAL_SECTION _mutex;
};

class MutexGuard
{
    NOCOPYASSIGN(MutexGuard);
public:
    MutexGuard(Mutex& mutex) : _mutex(mutex)
    {
        mutex.Enter();
    }

    ~MutexGuard()
    {
        _mutex.Leave();
    }
private:
    Mutex& _mutex;
};

NAMESPACE_CLOSE(TinyNet)