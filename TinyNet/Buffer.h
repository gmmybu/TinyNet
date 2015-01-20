#pragma once
#include "RefCount.h"

NAMESPACE_START(TinyNet)

class Buffer;
typedef SharedPtr<Buffer> BufferPtr;

class Buffer
{
    friend class Socket;
public:
    static BufferPtr Alloc(size_t size);

    void Write(const void* data, size_t size)
    {
        if (_base + size > _last)
            throw std::exception("Buffer::Write, Range Overflow");

        memcpy(_base, data, size);
        _base += size;
    }
private:
    uint8_t*   _base;
    uint8_t*   _last;
private:
    NOCOPYASSIGN(Buffer);
};

NAMESPACE_CLOSE(TinyNet)