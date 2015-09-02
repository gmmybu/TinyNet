#include "Packet.h"
#include "Buffer.h"


TINYNET_START()

PacketPtr Packet::Create(size_t capacity)
{
    if (capacity < MinCapacity) {
        capacity = MinCapacity;
    } else if (capacity > MaxCapacity) {
        capacity = MaxCapacity;
    }

    Packet* packet = (Packet*)malloc(sizeof(Packet) + capacity);
    packet->_size = capacity;
    packet->_used = 0;
    return PacketPtr(packet, free);
}

namespace {

Mutex __bufferContainersLock;
std::map<Packet*, RefCount<Buffer>*> __bufferContainers;

void Register(Packet* packet, RefCount<Buffer>* buffer)
{
    buffer->IncRef();

    MutexGuard guard(__bufferContainersLock);
    __bufferContainers.insert(std::make_pair(packet, buffer));
}

void UnRegister(Packet* packet)
{
    MutexGuard guard(__bufferContainersLock);
    auto iter = __bufferContainers.find(packet);
    if (iter != __bufferContainers.end()) {
        iter->second->DecRef();
        __bufferContainers.erase(iter);
    }
}

}

PacketPtr Packet::Create(RefCount<Buffer>* buffer, uint8_t* from)
{
    Packet* message = (Packet*)(from - 4);
    Register(message, buffer);

    return PacketPtr(message, UnRegister);
}

TINYNET_CLOSE()