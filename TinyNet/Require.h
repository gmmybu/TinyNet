#pragma once
#include <set>
#include <map>
#include <list>
#include <vector>
#include <memory>
#include <string>
#include <stdint.h>
#include <type_traits>
#include <algorithm>
#include <queue>
#include <condition_variable>

#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")


#define NOCOPYASSIGN(clazz)         \
    clazz(const clazz&);            \
    clazz& operator=(const clazz&);

#define TINYNET_START() namespace TinyNet {
#define TINYNET_CLOSE() }


TINYNET_START()

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

TINYNET_CLOSE()