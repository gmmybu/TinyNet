#include "Buffer.h"

NAMESPACE_START(TinyNet)

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

PacketPtr MakePacket(RefCount<Buffer>* buffer, uint8_t* from)
{
    Packet* message = (Packet*)(from - 4);
    Register(message, buffer);

    return PacketPtr(message, UnRegister);
}

BufferPtr BufferAllocator::Alloc(size_t size)
{
    if (size < 64) {
        size = 64;
    }

    Buffer* buffer = (Buffer*)malloc(sizeof(Buffer) + size + 1);
    buffer->_base = (uint8_t*)(buffer + 1);
    buffer->_last = buffer->_base + size;
    return BufferPtr(buffer, free);
}

NAMESPACE_CLOSE(TinyNet)