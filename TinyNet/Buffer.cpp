#include "Buffer.h"


TINYNET_START()

BufferPtr Buffer::Create(size_t size)
{
    if (size < 64) { size = 64; }

    Buffer* buffer = (Buffer*)malloc(sizeof(Buffer) + size);
    buffer->_base = (uint8_t*)(buffer + 1);
    buffer->_last = buffer->_base + size;
    return BufferPtr(buffer, free);
}

TINYNET_CLOSE()