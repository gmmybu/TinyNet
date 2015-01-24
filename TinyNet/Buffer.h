#pragma once
#include "RefCount.h"

NAMESPACE_START(TinyNet)

class Buffer;
typedef SharedPtr<Buffer> BufferPtr;

class Buffer
{
    NOCOPYASSIGN(Buffer);
public:
    static BufferPtr Create(size_t size);

    void Write(const void* data, size_t size)
    {
        if (_base + size > _last)
            throw std::exception("Buffer::Write, Range Overflow");

        memcpy(_base, data, size);
        _base += size;
    }

    uint8_t*   _base;
    uint8_t*   _last;
};

NAMESPACE_CLOSE(TinyNet)