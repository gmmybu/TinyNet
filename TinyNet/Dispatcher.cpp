#include "Dispatcher.h"

//可能锁的方式有问题
NAMESPACE_START(TinyNet)

void Dispatcher::Start()
{
    if (InterlockedCompareExchange(&_isRunning, 1, 0) == 0) {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        _threadCount = info.dwNumberOfProcessors;
        for (DWORD i = 0; i < _threadCount; i++) {
            _threads[i] = CreateThread(NULL, 0, Dispatcher::ThreadProc, 0, 0, NULL);
            SetThreadAffinityMask(_threads[i], i + 1);
        }
    }
}

void Dispatcher::Close()
{
    if (InterlockedCompareExchange(&_isRunning, 0, 1) == 1) {
        WaitForMultipleObjects(_threadCount, _threads, TRUE, INFINITE);
        
        MutexGuard guard(_lock);
        _hanlder2EventQueue.clear();
        _list.clear();
    }
}

void Dispatcher::Enqueue(const SocketEvent& socketEvent)
{
    MutexGuard guard(_lock);
    
    SocketEventQueuePtr socketEventQueue;
    if (socketEvent._handler.Get()) {
        auto iter = _hanlder2EventQueue.find(socketEvent._handler);
        if (iter != _hanlder2EventQueue.end()) {
            socketEventQueue = iter->second;
        } else {
            socketEventQueue = SocketEventQueuePtr(new SocketEventQueue);
            socketEventQueue->_work = false;
            socketEventQueue->_wait = false;
            _hanlder2EventQueue.insert(std::make_pair(socketEvent._handler, socketEventQueue));
        }
    } else {
        auto iter = _acceptHandler2EventQueue.find(socketEvent._acceptHandler);
        if (iter != _acceptHandler2EventQueue.end()) {
            socketEventQueue = iter->second;
        } else {
            socketEventQueue = SocketEventQueuePtr(new SocketEventQueue);
            socketEventQueue->_work = false;
            socketEventQueue->_wait = false;
            _acceptHandler2EventQueue.insert(std::make_pair(socketEvent._acceptHandler, socketEventQueue));
        }
    }
    
    {
        MutexGuard queueGuard(socketEventQueue->_lock);
        socketEventQueue->_list.push_back(std::move(socketEvent));
    }

    Enqueue(socketEventQueue, false);
}

void Dispatcher::Enqueue(SocketEventQueuePtr& socketEventQueue, bool resetFlag)
{
    MutexGuard guard(_lock);

    if (resetFlag) {
        socketEventQueue->_work = false;
    }
    if (socketEventQueue->_list.size() > 0 && !socketEventQueue->_wait && !socketEventQueue->_work) {
        socketEventQueue->_wait = true;

        printf("Dispatcher::Enqueue, push back\n");
        _list.push_back(socketEventQueue);
    }
}

SocketEventQueuePtr Dispatcher::Dequeue()
{
    MutexGuard guard(_lock);
    if (_list.empty())
        return SocketEventQueuePtr();

    SocketEventQueuePtr socketEventQueue = _list.front();
    _list.pop_front();

    socketEventQueue->_work = true;
    socketEventQueue->_wait = false;
    return socketEventQueue;
}

DWORD WINAPI Dispatcher::ThreadProc(LPVOID)
{
    Dispatcher& dispatcher = Dispatcher::Instance();
    while (dispatcher._isRunning) {
        SocketEventQueuePtr socketEventQueue = dispatcher.Dequeue();
        if (socketEventQueue.Get() != nullptr) {
            SocketEvent socketEvent;
            {
                MutexGuard guard(socketEventQueue->_lock);
                socketEvent = socketEventQueue->_list.front();
                socketEventQueue->_list.pop_front();
            }

            switch (socketEvent._type)
            {
            case Socket_Connect:
                socketEvent._handler->OnConnect(socketEvent._name, socketEvent._status);
                break;
            case Socket_Receive:
                socketEvent._handler->OnReceive(socketEvent._name, socketEvent._packet);
                break;
            case Socket_Close:
                if (socketEvent._handler.Get()) {
                    socketEvent._handler->OnClose(socketEvent._name);
                } else {
                    socketEvent._acceptHandler->OnClose(socketEvent._name);
                }
                break;
            default:
                throw std::exception("Dispatcher::ThreadProc: Unknown EventType");
                break;
            }
            dispatcher.Enqueue(socketEventQueue);
        } else {
            Sleep(1);
        }
    }
    return 0;
}

NAMESPACE_CLOSE(TinyNet)