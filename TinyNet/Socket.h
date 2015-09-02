#pragma once
#include "Packet.h"


TINYNET_START()

/// ignore send buffer overflow

/// callbacks will always be called except that when SocketManager is closing but some sockets are still active

/// you'd better close all sockets before close SocketManager

class SocketHandler
{
public:
    virtual ~SocketHandler() { }

    virtual void OnStart(uint32_t name, bool status) = 0;

    virtual void OnClose(uint32_t name) = 0;

    virtual void OnReceive(uint32_t name, PacketPtr& packet) = 0;
};

typedef SharedPtr<SocketHandler> SocketHandlerPtr;


class ServerHandler
{
public:
    virtual ~ServerHandler() { }

    virtual void OnClose(uint32_t name) = 0;

    /// return handler of incoming socket
    virtual SocketHandlerPtr OnAccept(uint32_t name) = 0;
};

typedef SharedPtr<ServerHandler> ServerHandlerPtr;


class Socket;

class SocketManager
{
    NOCOPYASSIGN(SocketManager);
public:
    static SocketManager& Instance()
    {
        static SocketManager instance;
        return instance;
    }

    SocketManager() :
        _running(0),
        _completion(NULL),
        _thread(NULL),
        _sending(false),
        _dirty(false)
    {
    }

    void Start(uint32_t numOfWorkThread = 0);
    void Close();
            
    uint32_t Listen(const std::string& addr, uint16_t port, ServerHandlerPtr& handler);

    uint32_t Create(const std::string& addr, uint16_t port, SocketHandlerPtr& handler);

    void Transfer(uint32_t name, PacketPtr& packet, bool close = false);

    void ShutDown(uint32_t name);
private:
    static DWORD WINAPI ThreadProc(LPVOID);

    void MainLoop();

    HANDLE      _completion;
    HANDLE      _thread;
    uint32_t    _running;
private:
    uint32_t AddSocket(RefCount<Socket>* refer);
    
    RefCount<Socket>* GetSocket(uint32_t name);
    
    Mutex       _socketsLock;   /// should use rwlock
    uint32_t    _socketsNext;
    std::map<uint32_t, RefCount<Socket>*>    _sockets;
private:
    struct SocketInfo
    {
        SocketInfo(uint32_t name, const std::string& addr, uint16_t port) :
            _name(name), _addr(addr), _port(port) { }

        uint32_t       _name;
        std::string    _addr;
        uint16_t       _port;
    };

    struct SocketSend
    {
        SocketSend(uint32_t name, PacketPtr& data, bool close) :
            _name(name), _data(data), _close(close) { }

        uint32_t     _name;
        PacketPtr    _data;
        bool         _close;
    };

    bool    _dirty;

    Mutex   _queueLock;

    std::vector<SocketInfo>    _listenQueue;

    std::vector<SocketInfo>    _connectQueue;

    std::vector<uint32_t>      _closeQueue;

    bool     _sending;
    
    Mutex    _sendLock;

    std::vector<SocketSend>    _sendQueue;

    friend class Socket;
};

#define theManager SocketManager::Instance()

TINYNET_CLOSE()