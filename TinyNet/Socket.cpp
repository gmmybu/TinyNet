#include "Socket.h"
#include "Buffer.h"
#include "Dispatcher.h"
#include <mswsock.h>

TINYNET_START()

LPFN_ACCEPTEX  AcceptEx;
LPFN_CONNECTEX ConnectEx;

HANDLE __ioport;

inline void Schedule(SocketEvent&& ev)
{
    theDispatcher.Enqueue(std::move(ev));
}

inline void ClearOverlapped(OVERLAPPED& overlapped)
{
    memset(&overlapped, 0, sizeof(OVERLAPPED));
}

inline sockaddr_in GetSockAddr()
{
    sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    return addr;
}

inline sockaddr_in GetSockAddr(const std::string& host, uint16_t port)
{
    sockaddr_in addr = {0};
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host.c_str());
    return addr;
}

//////////////////////////////////////////////////////////////////////

class Socket;

typedef RefCount<Socket> SocketRef;

/// internal class

class Socket
{
public:
    Socket(SOCKET socket) :
        _socket(socket), _connected(false), _closed(false),
        _sending(false), _sendOffset(0), _listen(false), _name(0)
    {
    }

    Socket(SOCKET socket, SocketHandlerPtr& handler) :
        _socket(socket), _handler(handler), _connected(false), _closed(false),
        _sending(false), _sendOffset(0), _listen(false), _name(0)
    {
    }

    Socket(SOCKET socket, ServerHandlerPtr& acceptHandler) :
        _socket(socket), _acceptHandler(acceptHandler), _closed(false),
        _sendOffset(0), _listen(true), _connected(false), _name(0)
    {
    }

    static bool Initialize()
    {
        SOCKET socket = Create();
        if (socket == INVALID_SOCKET)
            return false;
        
        GUID acceptEx  = WSAID_ACCEPTEX;
        GUID connectEx = WSAID_CONNECTEX;
        DWORD byteRead = 0;
        DWORD ctrlCode = SIO_GET_EXTENSION_FUNCTION_POINTER; 
        WSAIoctl(socket, ctrlCode, &acceptEx,  sizeof(GUID), &AcceptEx,  sizeof(AcceptEx),  &byteRead, 0, 0);
        WSAIoctl(socket, ctrlCode, &connectEx, sizeof(GUID), &ConnectEx, sizeof(ConnectEx), &byteRead, 0, 0);
        closesocket(socket);

        return AcceptEx != NULL && ConnectEx != NULL;
    }
    
    static SOCKET Create()
    {
        return WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    }

    #pragma region Operations
    
    bool Bind(HANDLE completion)
    {
        HANDLE fileHandle = (HANDLE)_socket;
        return CreateIoCompletionPort(fileHandle, completion, (ULONG_PTR)_self, 0) != NULL;
    }

    bool Bind(sockaddr_in& addr)
    {
        return bind(_socket, (const sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR;
    }

    bool Listen()
    {
        return listen(_socket, 32) != SOCKET_ERROR;
    }

    bool Accept(SOCKET accept)
    {
        ClearOverlapped(_recvOverlapped);
        
        DWORD size = sizeof(sockaddr_in) + 16;
        return Check(AcceptEx(_socket, accept, _acceptBuffer, 0, size, size, NULL, &_recvOverlapped));
    }

    bool Connect(sockaddr_in& addr)
    {
        ClearOverlapped(_sendOverlapped);
        return Check(ConnectEx(_socket, (const sockaddr*)&addr, sizeof(addr), NULL, 0, NULL, &_sendOverlapped));
    }

    bool Connect(const std::string& host, uint16_t port)
    {
        sockaddr_in addr = GetSockAddr(host, port);
        return Connect(addr); 
    }

    bool Send(void* data, size_t size)
    {
        _sendBuff.buf = (CHAR*)data;
        _sendBuff.len = size;

        ClearOverlapped(_sendOverlapped);

        return Check(WSASend(_socket, &_sendBuff, 1, NULL, 0, &_sendOverlapped, NULL) != SOCKET_ERROR);
    }

    bool Receive(void* data, size_t size)
    {
        _recvBuff.buf = (CHAR*)data;
        _recvBuff.len = size;

        ClearOverlapped(_recvOverlapped);

        DWORD RecvBytes, Flags = 0;
        return Check(WSARecv(_socket, &_recvBuff, 1, &RecvBytes, &Flags, &_recvOverlapped, NULL) != SOCKET_ERROR);
    }

    bool Check(BOOL status)
    {
        return (status || WSAGetLastError() == ERROR_IO_PENDING) && _self->IncRef();
    }

    #pragma endregion

    #pragma region Accept

    void DoAccept(const std::string& host, uint16_t port)
    {
        sockaddr_in addr = GetSockAddr(host, port);
        if (!Bind(addr) || !Bind(__ioport) || !Listen()) {
            theManager.ShutDown(_name);
        } else {
            BeginAccept();
        }
    }

    void OnAccept(BOOL status)
    {
        if (status) {
            auto refer = theManager.GetSocket(_accept);
            if (refer != nullptr) {
                Socket* socket = refer->Get();
                socket->_handler = _acceptHandler->OnAccept(_accept);
                if (socket->Bind(__ioport)) {
                    socket->_connected = true;
                    Schedule(SocketEvent::MakeConnect(socket->_handler, _accept, true));
                    socket->BeginReceive();
                } else {
                    theManager.ShutDown(_accept);
                }
            }
        } else {
            theManager.ShutDown(_accept);
        }

        BeginAccept();
    }

    void BeginAccept()
    {
        SOCKET accept = Socket::Create();
        if (accept == INVALID_SOCKET) {
            theManager.ShutDown(_name);
            return;
        }

        _accept = theManager.AddSocket(MakeShared(new Socket(accept)));
        if (!Accept(accept)) {
            theManager.ShutDown(_accept);
            theManager.ShutDown(_name);
        }
    }

    #pragma endregion

    #pragma region Connect

    void DoConnect(const std::string& host, uint16_t port)
    {
        sockaddr_in addr = GetSockAddr();
        if (!Bind(addr) || !Bind(__ioport) || !Connect(host, port)) {
            Schedule(SocketEvent::MakeConnect(_handler, _name, false));
            theManager.ShutDown(_name);
        }
    }

    void OnConnect(BOOL status)
    {
        if (status) {
            _connected = true;
            Schedule(SocketEvent::MakeConnect(_handler, _name, true));

            BeginSend();
            BeginReceive();
        } else {
            Schedule(SocketEvent::MakeConnect(_handler, _name, false));

            theManager.ShutDown(_name);
        }
    }

    #pragma endregion

    #pragma region Receive
    
    void OnReceive(uint32_t transfered)
    {
        if (transfered == 0) {
            theManager.ShutDown(_name);
            return;
        }

        _recvBuffer->_base += transfered;
        while (_recvBuffer->_base - _recvFrom >= 12) {
            size_t* used = (size_t*)_recvFrom;
            if (_recvBuffer->_base - _recvFrom >= *used + 12) {
                PacketPtr packet = Packet::Create(_recvBuffer.GetRef(), _recvFrom);
                Schedule(SocketEvent::MakeReceive(_handler, _name, packet));
                _recvFrom += *used + 12;
            } else {
                break;
            }
        }

        size_t newBufferSize = 0;
        if (_recvBuffer->_base - _recvFrom >= 4) {
            size_t* used = (size_t*)_recvFrom;
            //遇到大包直接断开
            if (*used >= 65500) {
                theManager.ShutDown(_name);
                return;
            }

            if (_recvBuffer->_last - _recvFrom < *used + 12) {
                newBufferSize = *used + 12 + 1024;
            }
        } else if (_recvBuffer->_last - _recvFrom < 128) {
            newBufferSize = 1024;
        }

        if (newBufferSize != 0) {
            BufferPtr newBuffer = Buffer::Create(newBufferSize);
            uint8_t* newStart = newBuffer->_base;

            newBuffer->Write(_recvFrom, _recvBuffer->_base - _recvFrom);
            _recvFrom = newStart;
            _recvBuffer = newBuffer;
        }

        BeginReceive();
    }

    void BeginReceive()
    {
        if (_recvBuffer.Get() == NULL) {
            _recvBuffer = Buffer::Create(2048);
            _recvFrom = _recvBuffer->_base;
        }

        if (!Receive(_recvBuffer->_base, _recvBuffer->_last - _recvBuffer->_base)) {
            theManager.ShutDown(_name);
        }
    }
    
    #pragma endregion

    #pragma region Send

    void DoSend(PacketPtr& packet, bool closing)
    {
        _closing = _closing || closing;

        _sendQueue.push_back(packet);
        if (!_sending && _connected) {
            BeginSend();
        }
    }

    void OnSend(uint32_t transfered)
    {
        if (transfered == 0) {
            theManager.ShutDown(_name);
            return;
        }

        _sendOffset += transfered;

        if (_sendPacket->_used + 12 == _sendOffset) {
            _sendPacket.Reset();
        }
        BeginSend();
    }

    void BeginSend()
    {
        if (_sendPacket.Get() == nullptr) {
            _sendOffset = 0;
            if (!_sendQueue.empty()) {
                _sendPacket = _sendQueue.front();
                _sendQueue.pop_front();
            }
        }

        if (_sendPacket.Get() != nullptr) {
            _sending = true;
            if (!Send((uint8_t*)&_sendPacket->_used + _sendOffset, _sendPacket->_used + 12 - _sendOffset)) {
                theManager.ShutDown(_name);
            }
        } else {
            _sending = false;
        }
    }

    #pragma endregion

    void DoClose()
    {
        if (!_closed) {
            if (_listen) {
                Schedule(SocketEvent::MakeClose(_acceptHandler, _name));
            } else if (_connected) {
                Schedule(SocketEvent::MakeClose(_handler, _name));
            }

            closesocket(_socket);
            _closed = true;
        }
    }

    //General
    SOCKET       _socket;
    uint32_t     _name;
    bool         _closed;
    bool         _listen;
    SocketRef*   _self;

    //Accept
    uint32_t            _accept;
    ServerHandlerPtr    _acceptHandler;

    //Connect
    bool                _connected;
    SocketHandlerPtr    _handler;

    //Receive
    uint8_t*     _recvFrom;
    BufferPtr    _recvBuffer;

    //Send
    bool         _sending;
    bool         _closing;
    uint32_t     _sendOffset;
    PacketPtr    _sendPacket;
    std::list<PacketPtr>    _sendQueue;

    //Internal
    WSABUF           _recvBuff;
    WSABUF           _sendBuff;
    WSAOVERLAPPED    _sendOverlapped;
    WSAOVERLAPPED    _recvOverlapped;
    CHAR             _acceptBuffer[128];

    static void DoPoll()
    {
        DWORD           transfered = 0;
        ULONG_PTR       completion = 0;
        LPOVERLAPPED    overlapped = nullptr;

        BOOL status = GetQueuedCompletionStatus(__ioport, &transfered,
            &completion, &overlapped, 1);

        if (overlapped != nullptr) {
            SocketRef* refer = (SocketRef*)completion;
            
            Socket* socket = refer->Get();

            /// assume after DoClose, [OnAccept, OnReceive, OnSend, OnConnect] all return failure
            if (overlapped == &socket->_recvOverlapped) {
                if (socket->_listen) {
                    socket->OnAccept(status);
                } else {
                    socket->OnReceive(transfered);
                }
            } else {
                if (socket->_connected) {
                    socket->OnSend(transfered);
                } else {
                    socket->OnConnect(status);
                }
            }
            refer->DecRef();
        }
    }

    static uint32_t DoWait()
    {
        DWORD           transfered = 0;
        ULONG_PTR       completion = 0;
        LPOVERLAPPED    overlapped = nullptr;

        GetQueuedCompletionStatus(__ioport, &transfered,
            &completion, &overlapped, 0);

        if (overlapped != nullptr) {
            SocketRef* refer = (SocketRef*)completion;
            return refer->DecRef();
        }
        return 0;
    }
};

//////////////////////////////////////////////////////////////////////

DWORD WINAPI SocketManager::ThreadProc(LPVOID)
{
    theManager.MainLoop();
    return 0;
}

void SocketManager::Start(uint32_t numOfWorkThread)
{
    if (InterlockedCompareExchange(&_running, 1, 0) == 0) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
            throw std::exception("SocketManager::Start, 1");

        if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
            throw std::exception("SocketManager::Start, 2");

        if (!Socket::Initialize())
            throw std::exception("SocketManager::Start, 3");

        _completion = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (_completion == NULL)
            throw std::exception("SocketManager::Start, 4");

        _thread = CreateThread(NULL, 0, &SocketManager::ThreadProc, NULL, 0, NULL);
        if (_thread == NULL)
            throw std::exception("SocketManager::Start, 6");

        __ioport = _completion;

        theDispatcher.Start(numOfWorkThread);
    }
}

void SocketManager::Close()
{
    theDispatcher.Close();

    if (InterlockedCompareExchange(&_running, 0, 1) == 1) {
        _dirty = true;

        if (_thread != NULL) {
            WaitForSingleObject(_thread, INFINITE);
            CloseHandle(_thread);
            _thread = NULL;
        }

        if (_completion != NULL) {
            CloseHandle(_completion);
            _completion = NULL;
        }

        WSACleanup();
    }
}

void SocketManager::MainLoop()
{
    while (_running) {
        Socket::DoPoll();

        if (_sending) {
            std::vector<SocketSend>    sendQueue;
            {
                MutexGuard guard(_sendLock);
                sendQueue = std::move(_sendQueue);
                _sendQueue.reserve(10000);
                _sending = false;
            }

            for (auto send : sendQueue) {
                auto refer = GetSocket(send._name);
                if (refer != nullptr) {
                    refer->Get()->DoSend(send._data, send._close);
                }
            }
        }

        if (_dirty) {
            std::vector<SocketInfo>    listenQueue;
            std::vector<SocketInfo>    connectQueue;
            std::vector<uint32_t>      closeQueue;
            {
                MutexGuard guard(_queueLock);
                listenQueue  = std::move(_listenQueue);
                connectQueue = std::move(_connectQueue);
                closeQueue   = std::move(_closeQueue);
                _dirty       = false;
            }

            for (auto name : closeQueue) {
                RefCount<Socket>* refer = nullptr;
                {
                    MutexGuard guard(_socketsLock);
                    auto iter = _sockets.find(name);
                    if (iter != _sockets.end()) {
                        refer = iter->second;
                        _sockets.erase(iter);
                    }
                }

                if (refer != nullptr) {
                    refer->Get()->DoClose();
                    refer->DecRef();
                }
            }

            for (auto info : listenQueue) {
                auto refer = GetSocket(info._name);
                if (refer != nullptr) {
                    refer->Get()->DoAccept(info._addr, info._port);
                }
            }

            for (auto info : connectQueue) {
                auto refer = GetSocket(info._name);
                if (refer != nullptr) {
                    refer->Get()->DoConnect(info._addr, info._port);
                }
            }
        }
    }

    /// close all sockets
    uint32_t pendingCount = 0;
    {
        MutexGuard guard(_socketsLock);
        for (auto socket : _sockets) {
            RefCount<Socket>* refer = socket.second;
            refer->Get()->DoClose();

            if (refer->GetRef() > 1) { pendingCount++; }
        }
    }

    /// wait for pending sockets, at most 5000ms 
    DWORD startTime = GetTickCount();
    while (pendingCount > 0 && GetTickCount() - startTime < 5000) {
        if (Socket::DoWait() == 1) { pendingCount--; }
    }

    /// clear sockets
    {
        MutexGuard guard(_socketsLock);
        for (auto socket : _sockets) {
            socket.second->DecRef();
        }
        _sockets.clear();
    }

    /// clear queues
    {
        MutexGuard guard(_queueLock);
        _listenQueue.clear();
        _connectQueue.clear();
        _sendQueue.clear();
        _closeQueue.clear();
    }
}

uint32_t SocketManager::AddSocket(RefCount<Socket>* refer)
{
    MutexGuard guard(_socketsLock);

    while (true) {
        uint32_t next = ++_socketsNext;
        if (next != 0 && _sockets.find(next) == _sockets.end()) {
            _sockets.insert(std::make_pair(next, refer));
            refer->Get()->_name = next;
            refer->Get()->_self = refer;
            return next;
        }
    }
    return 0;
}

RefCount<Socket>* SocketManager::GetSocket(uint32_t name)
{
    MutexGuard guard(_socketsLock);
    auto iter = _sockets.find(name);
    return iter != _sockets.end() ? iter->second : nullptr;
}

uint32_t SocketManager::Listen(const std::string& addr, uint16_t port, ServerHandlerPtr& handler)
{
    if (_running == 0) return 0;

    SOCKET socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (socket == INVALID_SOCKET) return 0;

    uint32_t name = AddSocket(MakeShared(new Socket(socket, handler)));

    MutexGuard guard(_queueLock);
    _listenQueue.emplace_back(name, addr, port);
    _dirty = true;

    return name;
}

uint32_t SocketManager::Create(const std::string& addr, uint16_t port, SocketHandlerPtr& handler)
{
    if (_running == 0) return 0;

    SOCKET socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (socket == INVALID_SOCKET) return 0;

    uint32_t name = AddSocket(MakeShared(new Socket(socket, handler)));

    MutexGuard guard(_queueLock);
    _connectQueue.emplace_back(name, addr, port);
    _dirty = true;
    
    return name;
}

void SocketManager::Transfer(uint32_t name, PacketPtr& packet, bool close)
{
    MutexGuard guard(_sendLock);
    _sendQueue.emplace_back(name, packet, close);
    _sending = true;
}

void SocketManager::ShutDown(uint32_t name)
{
    MutexGuard guard(_queueLock);
    _closeQueue.push_back(name);
    _dirty = true;
}

TINYNET_CLOSE()