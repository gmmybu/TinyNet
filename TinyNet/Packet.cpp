#include "Packet.h"
#include "Buffer.h"

NAMESPACE_START(TinyNet)

PacketPtr Packet::Create(size_t capacity)
{
    if (capacity < MinCapacity) {
        capacity = MinCapacity;
    } else if (capacity > MaxCapacity) {
        capacity = MaxCapacity;
    }

    Packet* message = (Packet*)malloc(sizeof(Packet) + capacity);
    message->_size = capacity;
    message->_used = 0;
    return PacketPtr(message, free);
}

static Mutex g_bufferContainersLock;
static std::map<Packet*, RefCount<Buffer>*> g_bufferContainers;

static void Register(Packet* message, RefCount<Buffer>* buffer)
{
    buffer->IncRef();

    MutexGuard guard(g_bufferContainersLock);
    g_bufferContainers.insert(std::make_pair(message, buffer));
}

static void UnRegister(Packet* message)
{
    MutexGuard guard(g_bufferContainersLock);
    auto iter = g_bufferContainers.find(message);
    if (iter != g_bufferContainers.end()) {
        iter->second->DecRef();
        g_bufferContainers.erase(iter);
    }
}

PacketPtr Packet::Create(RefCount<Buffer>* buffer, uint8_t* from)
{
    Packet* message = (Packet*)(from - 4);
    Register(message, buffer);

    return PacketPtr(message, UnRegister);
}

NAMESPACE_CLOSE(TinyNet)