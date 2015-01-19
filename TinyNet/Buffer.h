#pragma once
#include "Packet.h"

//�ڲ�ʹ��
NAMESPACE_START(TinyNet)
 
class Buffer
{
public:
    size_t GetCapacity()
    {
        return _last - _base;
    }

    //��������
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

//����ֻ������
PacketPtr MakePacket(RefCount<Buffer>* buffer, uint8_t* from);

NAMESPACE_CLOSE(TinyNet)