#include "Packet.h"
#include "Buffer.h"

NAMESPACE_START(TinyNet)

PacketPtr Packet::Alloc(size_t capacity)
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

static Mutex __ownersLock;
static std::map<Packet*, RefCount<Buffer>*> __owners;

static void Register(Packet* message, RefCount<Buffer>* buffer)
{
    buffer->IncRef();

    MutexGuard guard(__ownersLock);
    __owners.insert(std::make_pair(message, buffer));
}

static void UnRegister(Packet* message)
{
    MutexGuard guard(__ownersLock);
    auto iter = __owners.find(message);
    if (iter != __owners.end()) {
        iter->second->DecRef();
        __owners.erase(iter);
    }
}

PacketPtr Packet::Alloc(RefCount<Buffer>* buffer, uint8_t* from)
{
    Packet* message = (Packet*)(from - 4);
    Register(message, buffer);

    return PacketPtr(message, UnRegister);
}

NAMESPACE_CLOSE(TinyNet)