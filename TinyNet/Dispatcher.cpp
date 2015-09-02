#include "Dispatcher.h"


TINYNET_START()

void Dispatcher::Start(uint32_t threadCount)
{
    if (InterlockedCompareExchange(&_running, 1, 0) == 0) {
        if (threadCount == 0) {
            SYSTEM_INFO info;
            GetSystemInfo(&info);
            threadCount = info.dwNumberOfProcessors;
        }

        _threadCount = threadCount;
        for (uint32_t i = 0; i < _threadCount; i++) {
            _threads[i] = CreateThread(NULL, 0, Dispatcher::ThreadProc, 0, 0, NULL);
            if (_threads[i] == NULL)
                throw std::exception("Dispatcher::Start, 1");
        }
    }
}

void Dispatcher::Close()
{
    if (InterlockedCompareExchange(&_running, 0, 1) == 1) {
        WaitForMultipleObjects(_threadCount, _threads, TRUE, INFINITE);
        for (uint32_t i = 0; i < _threadCount; i++) {
            _threads[i] = NULL;
        }
        
        MutexGuard guard(_eventQueueLock);
        _socketHanlder2EventQueue.clear();
        _socketEventQueueList.clear();
        _serverEventQueue.Reset();
    }
}

void Dispatcher::Enqueue(SocketEvent&& socketEvent)
{
    MutexGuard guard(_eventQueueLock);
    
    SocketEventQueuePtr socketEventQueue;
    if (socketEvent._handler.Get()) {
        auto iter = _socketHanlder2EventQueue.find(socketEvent._handler);
        if (iter != _socketHanlder2EventQueue.end()) {
            socketEventQueue = iter->second;
        } else {
            socketEventQueue = SocketEventQueuePtr(new SocketEventQueue);
            socketEventQueue->_work = false;
            socketEventQueue->_wait = false;
            socketEventQueue->_active = 0;
            _socketHanlder2EventQueue.insert(std::make_pair(socketEvent._handler, socketEventQueue));
        }
    } else {
        if (_serverEventQueue.Get() == nullptr) {
            _serverEventQueue = SocketEventQueuePtr(new SocketEventQueue);
            _serverEventQueue->_work = false;
            _serverEventQueue->_wait = false;
            _serverEventQueue->_active = 1;
        }

        socketEventQueue = _serverEventQueue;
    }
    
    {
        MutexGuard queueGuard(socketEventQueue->_lock);
        socketEventQueue->_list.push_back(std::move(socketEvent));
    }

    Enqueue(socketEventQueue, false);
}

void Dispatcher::Enqueue(SocketEventQueuePtr& socketEventQueue, bool resetFlag)
{
    MutexGuard guard(_eventQueueLock);

    if (resetFlag) {
        socketEventQueue->_work = false;
    }

    if (socketEventQueue->_list.size() > 0 && !socketEventQueue->_wait && !socketEventQueue->_work) {
        socketEventQueue->_wait = true;
        _socketEventQueueList.push_back(socketEventQueue);
    }
}

SocketEventQueuePtr Dispatcher::Dequeue()
{
    MutexGuard guard(_eventQueueLock);
    if (_socketEventQueueList.empty())
        return SocketEventQueuePtr();

    SocketEventQueuePtr socketEventQueue = _socketEventQueueList.front();
    _socketEventQueueList.pop_front();

    socketEventQueue->_work = true;
    socketEventQueue->_wait = false;
    return socketEventQueue;
}

void Dispatcher::MainLoop()
{
    while (_running) {
        SocketEventQueuePtr socketEventQueue = Dequeue();
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
                socketEvent._handler->OnStart(socketEvent._name, socketEvent._status);
                socketEventQueue->_active++;
                break;
            case Socket_Receive:
                socketEvent._handler->OnReceive(socketEvent._name, socketEvent._packet);
                break;
            case Socket_Close:
                if (socketEvent._handler.Get()) {
                    socketEvent._handler->OnClose(socketEvent._name);
                    socketEventQueue->_active--;

                    if (socketEventQueue->_active == 0) {
                        MutexGuard guard(_eventQueueLock);
                        if (socketEventQueue->_active == 0 && socketEventQueue->_list.empty()) {
                            _socketHanlder2EventQueue.erase(socketEvent._handler);
                        }
                    }
                } else {
                    socketEvent._serverHandler->OnClose(socketEvent._name);
                }
                break;
            default:
                throw std::exception("Dispatcher::ThreadProc, Unknown EventType");
                break;
            }
            Enqueue(socketEventQueue);
        } else {
            /// should use condition variable
            Sleep(1);
        }
    }
}

DWORD WINAPI Dispatcher::ThreadProc(LPVOID)
{
    theDispatcher.MainLoop();
    return 0;
}

TINYNET_CLOSE()