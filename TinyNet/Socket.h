#pragma once
#include "Packet.h"

NAMESPACE_START(TinyNet)

class SocketHandler
{
public:
    virtual ~SocketHandler() { }

    virtual void OnConnect(uint32_t name, bool status) = 0;

    virtual void OnClose(uint32_t name) = 0;

    virtual void OnReceive(uint32_t name, PacketPtr& packet) = 0;
};

typedef SharedPtr<SocketHandler> SocketHandlerPtr;

class SocketAcceptHandler
{
public:
    virtual ~SocketAcceptHandler() { }

    virtual void OnClose(uint32_t name) = 0;

    virtual SocketHandlerPtr GetHandler(uint32_t name) = 0;
};

typedef SharedPtr<SocketAcceptHandler> SocketAcceptHandlerPtr;

class Socket;

class SocketManager
{
    friend class Socket;
public:
    static SocketManager& Instance()
    {
        static SocketManager manager;
        return manager;
    }

    SocketManager() : _running(0)
    {
    }

    void Start();
    void Close();

    uint32_t Listen(const std::string& addr, uint16_t port, SocketAcceptHandlerPtr& handler);
    
    uint32_t Connect(const std::string& addr, uint16_t port, SocketHandlerPtr& handler);

    void SendPacket(uint32_t name, PacketPtr& packet, bool closeWhenComplete = false);

    void CloseSocket(uint32_t name);
private:
    static DWORD WINAPI ThreadProc(LPVOID);

    RefCount<Socket>* GetSocket(uint32_t name);
    uint32_t AddSocket(RefCount<Socket>* refer);

    NOCOPYASSIGN(SocketManager);
private:
    HANDLE               _completion;
    HANDLE               _thread;
    volatile uint32_t    _running;

    Mutex                                    _socketsLock;
    volatile uint32_t                        _socketsNext;
    std::map<uint32_t, RefCount<Socket>*>    _sockets;


    bool    _dirty;
    Mutex   _queueLock;

    struct ListenData
    {
        ListenData(uint32_t name, const std::string& addr, uint16_t port) :
            _name(name), _addr(addr), _port(port)
        {
        }

        uint32_t       _name;
        std::string    _addr;
        uint16_t       _port;
    };

    std::list<ListenData>    _listenQueue;

    struct ConnectData
    {
        ConnectData(uint32_t name, const std::string& addr, uint16_t port) :
            _name(name), _addr(addr), _port(port)
        {
        }

        uint32_t       _name;
        std::string    _addr;
        uint16_t       _port;
    };

    std::list<ConnectData>    _connectQueue;

    struct SendData
    {
        SendData(uint32_t name, PacketPtr& data, bool closeOnComplete) :
            _name(name), _data(data), _closeOnComplete(closeOnComplete)
        {
        }

        uint32_t     _name;
        PacketPtr    _data;
        bool         _closeOnComplete;
    };

    std::list<SendData>    _sendQueue;

    std::list<uint32_t>    _closeQueue;
};

}