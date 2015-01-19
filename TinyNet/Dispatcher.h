#pragma once
#include "Socket.h"

//内部使用
NAMESPACE_START(TinyNet)

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

    static SocketEvent MakeClose(SocketAcceptHandlerPtr& acceptHanlder, uint32_t name)
    {
        SocketEvent se;
        se._type = Socket_Close;
        se._name = name;
        se._acceptHandler = acceptHanlder;
        return se;
    }

    SocketEventType           _type;
    uint32_t                  _name;

    bool                      _status;  //for connect

    PacketPtr                 _packet;  //for receive

    SocketHandlerPtr          _handler;
    SocketAcceptHandlerPtr    _acceptHandler;
};

struct SocketEventQueue
{
    bool                      _wait;    //是否在队列中
    bool                      _work;    //是否在处理中
    Mutex                     _lock;
    std::list<SocketEvent>    _list; 
};

typedef SharedPtr<SocketEventQueue> SocketEventQueuePtr;  

//暂用互斥量同步
class Dispatcher
{
public:
    static Dispatcher& Instance()
    {
        static Dispatcher instance;
        return instance;
    }

    Dispatcher() :
        _threadCount(0), _isRunning(0)
    {
    }

    void Start();
    void Close();

    void Enqueue(const SocketEvent& socketEvent);
private:
    void Enqueue(SocketEventQueuePtr& socketEventQueue, bool resetFlag = true);
    SocketEventQueuePtr Dequeue();

    static DWORD WINAPI ThreadProc(LPVOID);
private:
    DWORD                _threadCount;
    HANDLE               _threads[32];
    volatile uint32_t    _isRunning;
    
    class SocketHandlerComparer
    {
    public:
        bool operator()(const SocketHandlerPtr& lhs, const SocketHandlerPtr& rhs) const
        {
            return lhs.Get() < rhs.Get();
        }
    };

    class SocketAcceptHandlerComparer
    {
    public:
        bool operator()(const SocketAcceptHandlerPtr& lhs, const SocketAcceptHandlerPtr& rhs) const
        {
            return lhs.Get() < rhs.Get();
        }
    };

    typedef std::map<SocketHandlerPtr, SocketEventQueuePtr, SocketHandlerComparer> Handler2EventQueueMap;
    typedef std::map<SocketAcceptHandlerPtr, SocketEventQueuePtr, SocketAcceptHandlerComparer> AcceptHandler2EventQueueMap;

    Mutex                             _lock;
    std::list<SocketEventQueuePtr>    _list;
    Handler2EventQueueMap             _hanlder2EventQueue;
    AcceptHandler2EventQueueMap       _acceptHandler2EventQueue;

    NOCOPYASSIGN(Dispatcher);
};

NAMESPACE_CLOSE(TinyNet)
