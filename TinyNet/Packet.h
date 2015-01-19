#pragma once
#include "RefCount.h"

NAMESPACE_START(TinyNet)

class Packet
{
public:
    size_t     _size;   //读数据时不要使用该字段
    size_t     _used;

    int32_t    _type;
    int32_t    _guid;
private:
    NOCOPYASSIGN(Packet);
};

typedef SharedPtr<Packet> PacketPtr;

class PacketAllocator
{
public:
    static const size_t MaxCapacity = INT16_MAX;
    static const size_t MinCapacity = 16;
    static const size_t DefCapacity = 128;
    static const size_t IncCapacity = 128;

    static PacketPtr Alloc(size_t capacity = DefCapacity);
};

enum SeekMode
{
    Seek_Set,
    Seek_Cur,
    Seek_End
};

class PacketReader
{
public:
    PacketReader(PacketPtr packet) : _packet(packet)
    {
        _data = _base = (uint8_t*)(_packet.Get() + 1);
        _last = _data + _packet->_used;
    }

    int32_t GetType() const
    {
        return _packet->_type;
    }

    int32_t GetGuid() const
    {
        return _packet->_guid;
    }

    size_t Seek(int32_t val, SeekMode mode)
    {
        switch (mode)
        {
        case SeekMode::Seek_Set:
            _base = _data + val;
            break;
        case SeekMode::Seek_Cur:
            _base = _base + val;
            break;
        case SeekMode::Seek_End:
            _base = _last + val;
            break;
        default:
            throw std::exception("MessageReader::Seek, Invalid SeekMode");
        }

        if (_base < _data || _base > _last)
            throw std::exception("MessageReader::Seek, Range Overflow");

        return _base - _data;
    }

    void Read(void* data, size_t size)
    {
        if (_base + size > _last)
            throw std::exception("MessageReader::Read, Range Overflow 0");

        memcpy(data, _base, size);
        _base += size;
    }

    template<class T>
    PacketReader& operator>>(T& val)
    {
        static_assert(!std::is_pointer<T>::value && !std::is_reference<T>::value && std::is_pod<T>::value, "Invalid Type");

        Read(&val, sizeof(T));
        return *this;
    }

    //假定Vector内存连续
    template<class T>
    PacketReader& operator>>(std::vector<T>& arr)
    {
        static_assert(!std::is_pointer<T>::value && !std::is_reference<T>::value && std::is_pod<T>::value, "Invalid Type");

        size_t size;
        operator>>(size);

        arr.resize(size);
        Read(arr.data(), sizeof(T) * size);
        return *this;
    }

    PacketReader& operator>>(std::string& text)
    {
        text = ReadString();
        return *this;
    }

    template<class T>
    const T* ReadArray(size_t& size)
    {
        operator>>(size);

        const T* arr = (const T*)_base; 
        if (sizeof(T) * size + _base > _last)
            throw std::exception("MessageReader::Read, Range Overflow 1");

        _base += size * sizeof(T);
        return arr;
    }

    //UTF_8编码，兼容C字符串
    const char* ReadString(size_t& size)
    {
        operator>>(size);

        if (size + 1 + _base > _last)
            throw std::exception("MessageReader::Read, Range Overflow 2");

        const char* text = (const char*)_base;
        if (text[size] != 0)
            throw std::exception("MessageReader::ReadString, Bad String");

        _base += size + 1;
        return text;
    }

    const char* ReadString()
    {
        size_t size;
        return ReadString(size);
    }
private:
    uint8_t*     _data;
    uint8_t*     _base;
    uint8_t*     _last;
    PacketPtr    _packet;
};

class PacketWriter
{
public:
    PacketWriter(int32_t type, int32_t guid, size_t capacity = PacketAllocator::DefCapacity)
    {
        _packet = PacketAllocator::Alloc(capacity);
        _packet->_type = type;
        _packet->_guid = guid;

        _base = (uint8_t*)(_packet.Get() + 1);
    }

    PacketPtr GetPacket()
    {
        return _packet;
    }

    void Write(const void* data, size_t size)
    {
        size_t used = _packet->_used + size;

        //扩充容量
        if (_packet->_size < used) {
            if (used > PacketAllocator::MaxCapacity)
                throw std::exception("MessageWriter::Write, Exceed MaxSize");    

            PacketPtr packet = PacketAllocator::Alloc(used + PacketAllocator::IncCapacity);
            memcpy(&packet->_used, &_packet->_used, _packet->_used + 12);

            _base = (uint8_t*)(packet.Get() + 1) + packet->_used;
            _packet = packet;
        }

        memcpy(_base, data, size);
        _base += size;
        _packet->_used += size;
    }

    template<class T>
    PacketWriter& operator<<(const T& val)
    {
        static_assert(!std::is_pointer<T>::value && !std::is_reference<T>::value && std::is_pod<T>::value, "Invalid Type");

        Write(&val, sizeof(T));
        return *this;
    }

    template<class T>
    PacketWriter& operator<<(const std::vector<T>& arr)
    {
        static_assert(!std::is_pointer<T>::value && !std::is_reference<T>::value && std::is_pod<T>::value, "Invalid Type");

        operator<<(arr.size());
        Write(arr.data(), arr.size() * sizeof(T));
        return *this;
    }

    PacketWriter& operator<<(const std::string& text)
    {
        operator<<(text.size());
        Write(text.c_str(), text.size() + 1);
        return *this;
    }
private:
    uint8_t*     _base;
    PacketPtr    _packet;
private:
    NOCOPYASSIGN(PacketWriter);
};

NAMESPACE_CLOSE(TinyNet)