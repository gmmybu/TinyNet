#pragma once
#include "Packet.h"

//内部使用
NAMESPACE_START(TinyNet)
 
class Buffer
{
public:
    size_t GetCapacity()
    {
        return _last - _base;
    }

    //基本不用
    void Write(const void* data, size_t size)
    {
        if (_base + size > _last)
            throw std::exception("Buffer::Write, Range Overflow");

        memcpy(_base, data, size);
        _base += size;
    }

    uint8_t*  _base;
    uint8_t*  _last;
private:
    NOCOPYASSIGN(Buffer);
};

typedef SharedPtr<Buffer> BufferPtr;  

class BufferAllocator
{
public:
    static BufferPtr Alloc(size_t size);
};

//返回只读数据
PacketPtr MakePacket(RefCount<Buffer>* buffer, uint8_t* from);

NAMESPACE_CLOSE(TinyNet)