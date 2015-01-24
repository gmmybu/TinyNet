#include "Buffer.h"

NAMESPACE_START(TinyNet)

BufferPtr Buffer::Create(size_t size)
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