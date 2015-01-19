#include "Packet.h"

NAMESPACE_START(TinyNet)

PacketPtr PacketAllocator::Alloc(size_t capacity)
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

NAMESPACE_CLOSE(TinyNet)