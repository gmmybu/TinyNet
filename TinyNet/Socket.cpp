#include "Socket.h"
#include "Buffer.h"
#include "Dispatcher.h"

NAMESPACE_START(TinyNet)

LPFN_ACCEPTEX AcceptEx;
LPFN_CONNECTEX ConnectEx;

HANDLE g_Completion;
SocketManager& g_Manager = SocketManager::Instance();

inline void Schedule(const SocketEvent& ev)
{
    Dispatcher::Instance().Enqueue(ev);
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

class Socket
{
    friend class SocketManager;
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

    Socket(SOCKET socket, SocketAcceptHandlerPtr& acceptHandler) :
        _socket(socket), _acceptHandler(acceptHandler), _closed(false),
        _sendOffset(0), _listen(true), _connected(false), _name(0)
    {
    }
public:
    static bool Initialize()
    {
        SOCKET socket = Create();
        if (socket == INVALID_SOCKET)
            return false;
        
        GUID acceptEx = WSAID_ACCEPTEX;
        GUID connectEx = WSAID_CONNECTEX;
        DWORD byteRead = 0;
        DWORD ctrlCode = SIO_GET_EXTENSION_FUNCTION_POINTER; 
        WSAIoctl(socket, ctrlCode, &acceptEx, sizeof(GUID), &AcceptEx, sizeof(AcceptEx), &byteRead, 0, 0);
        WSAIoctl(socket, ctrlCode, &connectEx, sizeof(GUID), &ConnectEx, sizeof(ConnectEx), &byteRead, 0, 0);
        closesocket(socket);

        return AcceptEx != NULL && ConnectEx != NULL;
    }
    
    static SOCKET Create()
    {
        return WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    }
private:

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

        printf("Socket::Send, %x:%d\n", data, size);
        return Check(WSASend(_socket, &_sendBuff, 1, NULL, 0, &_sendOverlapped, NULL) != SOCKET_ERROR);
    }

    bool Receive(void* data, size_t size)
    {
        _recvBuff.buf = (CHAR*)data;
        _recvBuff.len = size;

        ClearOverlapped(_recvOverlapped);
        
        printf("Socket::Receive, %x:%d\n", data, size);

        DWORD RecvBytes, Flags = 0;
        return Check(WSARecv(_socket, &_recvBuff, 1, &RecvBytes, &Flags, &_recvOverlapped, NULL) != SOCKET_ERROR);
    }

    bool Check(BOOL status)
    {
        return (status || WSAGetLastError() == ERROR_IO_PENDING) && _self->IncRef();
    }

    #pragma endregion

private:

    #pragma region Accept

    void DoAccept(const std::string& host, uint16_t port)
    {
        sockaddr_in addr = GetSockAddr(host, port);
        if (!Bind(addr) || !Bind(g_Completion) || !Listen()) {
            printf("Socket::DoAccept, %d\n", GetLastError());
            g_Manager.CloseSocket(_name);
        } else {
            BeginAccept();
        }
    }

    void OnAccept(BOOL status)
    {
        printf("Socket::OnAccpet, %d\n", _accept);

        if (status) {
            auto refer = g_Manager.GetSocket(_accept);
            if (refer != nullptr) {
                Socket* socket = refer->Get();
                socket->_handler = _acceptHandler->GetHandler(_accept);
                socket->_connected = true;
                if (socket->Bind(g_Completion)) {
                    Schedule(SocketEvent::MakeConnect(socket->_handler, _accept, true));
                    socket->BeginReceive();
                } else {
                    g_Manager.CloseSocket(_accept);
                }
            }
        } else {
            g_Manager.CloseSocket(_accept);
        }

        BeginAccept();
    }

    void BeginAccept()
    {
        SOCKET accept = Socket::Create();
        if (accept == INVALID_SOCKET) {
            g_Manager.CloseSocket(_name);
            return;
        }

        _accept = g_Manager.AddSocket(MakeShared(new Socket(accept)));
        if (!Accept(accept)) {
            printf("Socket::BeginAccept, %d\n", GetLastError());
            g_Manager.CloseSocket(_accept);
            g_Manager.CloseSocket(_name);
        }
    }

    #pragma endregion

    #pragma region Connect

    void DoConnect(const std::string& host, uint16_t port)
    {
        sockaddr_in addr = GetSockAddr();
        if (!Bind(addr) || !Bind(g_Completion) || !Connect(host, port)) {
            g_Manager.CloseSocket(_name);
        }
    }

    void OnConnect(BOOL status)
    {
        printf("Socket::OnConnect, %d, %d\n", _name, status);

        Schedule(SocketEvent::MakeConnect(_handler, _name, status));

        _connected = status;
        if (_connected) {
            BeginSend();
            BeginReceive();
        } else {
            g_Manager.CloseSocket(_name);
        }
    }

    #pragma endregion

    #pragma region Receive
    
    void OnReceive(uint32_t transfered)
    {
        printf("Socket::OnReceive, %d\n", transfered);

        if (transfered == 0) {
            g_Manager.CloseSocket(_name);
            return;
        }

        _recvBuffer->_base += transfered;
        while (_recvBuffer->_base - _recvFrom >= 12) {
            size_t* used = (size_t*)_recvFrom;
            if (_recvBuffer->_base - _recvFrom >= *used + 12) {
                PacketPtr packet = Packet::Alloc(_recvBuffer.GetRef(), _recvFrom);
                Schedule(SocketEvent::MakeReceive(_handler, _name, packet));
                _recvFrom += *used + 12;
            } else {
                break;
            }
        }

        size_t newBufferSize = 0;
        if (_recvBuffer->_base - _recvFrom >= 4) {
            size_t* used = (size_t*)_recvFrom;
            //应该防止包头攻击
            if (_recvBuffer->_last - _recvFrom < *used + 12) {
                newBufferSize = *used + 12 + 1024;
            }
        } else if (_recvBuffer->_last - _recvFrom < 128) {
            newBufferSize = 1024;
        }

        if (newBufferSize != 0) {
            BufferPtr newBuffer = Buffer::Alloc(newBufferSize);
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
            _recvBuffer = Buffer::Alloc(2048);
            _recvFrom = _recvBuffer->_base;
        }

        if (!Receive(_recvBuffer->_base, _recvBuffer->_last - _recvBuffer->_base)) {
            printf("Socket::BeginReceive, %d\n", GetLastError());
            g_Manager.CloseSocket(_name);
        } else {
            printf("Socket::BeginReceive, success\n");
        }
    }
    
    #pragma endregion

    #pragma region Send

    void DoSend(PacketPtr& packet, bool closing)
    {
        printf("Socket::DoSend\n");

        _closing = _closing || closing;

        _sendQueue.push_back(packet);
        if (!_sending && _connected) {
            BeginSend();
        }
    }

    void OnSend(uint32_t transfered)
    {
        printf("Socket::OnSend, %d\n", transfered);

        if (transfered == 0) {
            g_Manager.CloseSocket(_name);
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
                printf("Socket::BeginSend, %d\n", GetLastError());
                g_Manager.CloseSocket(_name);
            } else {
                printf("Socket::BeginSend, success\n");
            }
        } else {
            _sending = false;
        }
    }

    #pragma endregion

    void DoClose()
    {
        printf("Socket::DoClose, %d\n", _name);

        if (_listen) {
            auto closeEvent = SocketEvent::MakeClose(_acceptHandler, _name);
            Schedule(closeEvent);
        } else if (_connected) {
            Schedule(SocketEvent::MakeClose(_handler, _name));
        }

        closesocket(_socket);
    }
private:
    //General
    SOCKET              _socket;
    uint32_t            _name;
    bool                _closed;
    bool                _listen;
    RefCount<Socket>*   _self;

    //Accept
    uint32_t                  _accept;
    SocketAcceptHandlerPtr    _acceptHandler;

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
};

//////////////////////////////////////////////////////////////////////

DWORD WINAPI SocketManager::ThreadProc(LPVOID)
{   
    bool& dirty   = g_Manager._dirty;
    auto& sockets = g_Manager._sockets;
    auto  completion = g_Manager._completion;

    while (true) {
        while (!dirty) {
            DWORD               transfered = 0;
            WSAOVERLAPPED*      overlapped = NULL;
            RefCount<Socket>*   refer = NULL;

            BOOL status = GetQueuedCompletionStatus(completion, &transfered,
                (PULONG_PTR)&refer, &overlapped, 0);

            if (!status && overlapped == NULL) {
                DWORD error = GetLastError();
                if (error == ERROR_ABANDONED_WAIT_0 || error == ERROR_INVALID_HANDLE) {
                    printf("SocketManager::Thread, Exit\n");
                    return 0;
                }
            }

            if (overlapped != NULL) {
                Socket* socket = refer->Get();
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

        std::list<ListenData>     listenQueue;
        std::list<ConnectData>    connectQueue;
        std::list<SendData>       sendQueue;
        std::list<uint32_t>       closeQueue;
        {
            MutexGuard guard(g_Manager._queueLock);
            listenQueue  = std::move(g_Manager._listenQueue);
            connectQueue = std::move(g_Manager._connectQueue);
            sendQueue    = std::move(g_Manager._sendQueue);
            closeQueue   = std::move(g_Manager._closeQueue);
            dirty        = false;
        }

        for (auto name : closeQueue) {
            RefCount<Socket>* refer = nullptr;
            {
                MutexGuard guard(g_Manager._socketsLock);
                auto iter = sockets.find(name);
                if (iter != sockets.end()) {
                    refer = iter->second;
                    sockets.erase(iter);
                }
            }
            if (refer != nullptr) {
                refer->Get()->DoClose();
                refer->DecRef();
            }
        }

        for (auto listenData : listenQueue) {
            auto refer = g_Manager.GetSocket(listenData._name);
            if (refer != nullptr) {
                refer->Get()->DoAccept(listenData._addr, listenData._port);
            }
        }

        for (auto connectData : connectQueue) {
            auto refer = g_Manager.GetSocket(connectData._name);
            if (refer != nullptr) {
                refer->Get()->DoConnect(connectData._addr, connectData._port);
            }
        }

        for (auto sendData : sendQueue) {
            auto refer = g_Manager.GetSocket(sendData._name);
            if (refer != nullptr) {
                refer->Get()->DoSend(sendData._data, sendData._closeOnComplete);
            }
        }
    }
    return 0;
}

void SocketManager::Start()
{
    if (InterlockedCompareExchange(&_running, 1, 0) == 0) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
            throw std::exception("SocketManager::Start, 1");

        if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
            WSACleanup();
            throw std::exception("SocketManager::Start, 2");
        }

        if (!Socket::Initialize())
            throw std::exception("SocketManager::Start, 3");

        _completion = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (_completion == NULL)
            throw std::exception("SocketManager::Start, 5");

        _thread = CreateThread(NULL, 0, &SocketManager::ThreadProc, NULL, 0, NULL);
        if (_thread == NULL)
            throw std::exception("SocketManager::Start, 6");

        g_Completion = _completion;

        Dispatcher::Instance().Start();

    }
}

void SocketManager::Close()
{
    if (InterlockedCompareExchange(&_running, 0, 1) == 1) {
        Dispatcher::Instance().Close();

        if (_completion != NULL) {
            CloseHandle(_completion);
            _completion = NULL;

            if (_thread != NULL) {
                WaitForSingleObject(_thread, INFINITE);
                CloseHandle(_thread);
                _thread = NULL;
            }
        }

        WSACleanup();
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

uint32_t SocketManager::Listen(const std::string& addr, uint16_t port, SocketAcceptHandlerPtr& acceptHandler)
{
    SOCKET socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (socket == INVALID_SOCKET) return 0;

    uint32_t   name = AddSocket(MakeShared(new Socket(socket, acceptHandler)));
    MutexGuard guard(_queueLock);
    _listenQueue.push_back(ListenData(name, addr, port));
    _dirty = true;
    return name;
}

uint32_t SocketManager::Connect(const std::string& addr, uint16_t port, SocketHandlerPtr& handler)
{
    SOCKET socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (socket == INVALID_SOCKET) return 0;

    uint32_t   name = AddSocket(MakeShared(new Socket(socket, handler)));
    MutexGuard guard(_queueLock);
    _connectQueue.push_back(ConnectData(name, addr, port));
    _dirty = true;
    return name;
}

void SocketManager::SendPacket(uint32_t name, PacketPtr& packet, bool closeWhenComplete)
{
    MutexGuard guard(_queueLock);
    _sendQueue.push_back(SendData(name, packet, closeWhenComplete));
    _dirty = true;
}

void SocketManager::CloseSocket(uint32_t name)
{
    MutexGuard guard(_queueLock);
    _closeQueue.push_back(name);
    _dirty = true;
}

NAMESPACE_CLOSE(TinyNet)