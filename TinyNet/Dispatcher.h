#pragma once
#include "Socket.h"


TINYNET_START()

enum SocketEventType
{
    Socket_Connect,
    Socket_Receive,
    Socket_Close,
};


struct SocketEvent
{
public:
    static SocketEvent MakeConnect(SocketHandlerPtr& handler, uint32_t name, bool status)
    {
        SocketEvent se;
        se._type = Socket_Connect;
        se._name = name;
        se._status  = status;
        se._handler = handler;
        return se;
    }

    static SocketEvent MakeReceive(SocketHandlerPtr& handler, uint32_t name, PacketPtr& packet)
    {
        SocketEvent se;
        se._type = Socket_Receive;
        se._name = name;
        se._handler = handler;
        se._packet  = packet;
        return se;
    }

    static SocketEvent MakeClose(SocketHandlerPtr& handler, uint32_t name)
    {
        SocketEvent se;
        se._type = Socket_Close;
        se._name = name;
        se._handler = handler;
        return se;
    }

    static SocketEvent MakeClose(ServerHandlerPtr& serverHandler, uint32_t name)
    {
        SocketEvent se;
        se._type = Socket_Close;
        se._name = name;
        se._serverHandler = serverHandler;
        return se;
    }

    SocketEventType     _type;
    uint32_t            _name;

    bool                _status;  //for connect

    PacketPtr           _packet;  //for receive

    SocketHandlerPtr    _handler;
    ServerHandlerPtr    _serverHandler;
};


struct SocketEventQueue
{
    bool                      _wait;    //是否在队列中
    bool                      _work;    //是否在处理中
    Mutex                     _lock;
    std::list<SocketEvent>    _list;

    /// numbers of active sockets
    uint32_t                  _active;
};

typedef SharedPtr<SocketEventQueue> SocketEventQueuePtr;  


class Dispatcher
{
    NOCOPYASSIGN(Dispatcher);
public:
    static Dispatcher& Instance()
    {
        static Dispatcher instance;
        return instance;
    }

    Dispatcher() : _threadCount(0), _running(0)
    {
        for (int i = 0; i < 32; i++) {
            _threads[i] = NULL;
        }
    }

    void Start(uint32_t threadCount = 0);
    void Close();

    void Enqueue(SocketEvent&& socketEvent);
private:
    void Enqueue(SocketEventQueuePtr& socketEventQueue, bool resetFlag = true);
    SocketEventQueuePtr Dequeue();

    static DWORD WINAPI ThreadProc(LPVOID);

    void MainLoop();
private:
    uint32_t    _threadCount;
    HANDLE      _threads[32];
    uint32_t    _running;
    
    class SocketHandlerComparer
    {
    public:
        bool operator()(const SocketHandlerPtr& lhs, const SocketHandlerPtr& rhs) const
        {
            return lhs.Get() < rhs.Get();
        }
    };

    typedef std::list<SocketEventQueuePtr> SocketEventQueuePtrList;
    typedef std::map<SocketHandlerPtr, SocketEventQueuePtr, SocketHandlerComparer> SocketHandler2EventQueueMap;

    /// should use condition variable

    Mutex                          _eventQueueLock;
    SocketEventQueuePtr            _serverEventQueue;
    SocketEventQueuePtrList        _socketEventQueueList;
    SocketHandler2EventQueueMap    _socketHanlder2EventQueue;
};

#define theDispatcher Dispatcher::Instance()

TINYNET_CLOSE()
