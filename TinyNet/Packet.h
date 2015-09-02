#pragma once
#include "RefCount.h"


TINYNET_START()

class Buffer;

class Packet;
typedef SharedPtr<Packet> PacketPtr;

class Packet
{
    NOCOPYASSIGN(Packet);
public:
    static const size_t MaxCapacity = INT16_MAX;
    static const size_t MinCapacity = 16;
    static const size_t DefCapacity = 128;
    static const size_t IncCapacity = 128;

    static PacketPtr Create(size_t capacity = DefCapacity);

    static PacketPtr Create(RefCount<Buffer>* buffer, uint8_t* from);

    size_t     _size;
    size_t     _used;

    int32_t    _type;
    int32_t    _guid;
};


enum SeekMode
{
    Seek_Set,
    Seek_Cur,
    Seek_End
};


class PacketReader
{
    NOCOPYASSIGN(PacketReader);
public:
    PacketReader(const PacketPtr& packet) : _packet(packet)
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
            throw std::exception("PacketReader::Seek, Invalid SeekMode");
        }

        if (_base < _data || _base > _last)
            throw std::exception("PacketReader::Seek, Range Overflow");

        return _base - _data;
    }

    void Read(void* data, size_t size)
    {
        if (_base + size > _last)
            throw std::exception("PacketReader::Read, Range Overflow 0");

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
            throw std::exception("PacketReader::Read, Range Overflow 1");

        _base += size * sizeof(T);
        return arr;
    }

    //UTF_8±àÂë£¬¼æÈÝC×Ö·û´®
    const char* ReadString(size_t& size)
    {
        operator>>(size);

        if (size + 1 + _base > _last)
            throw std::exception("PacketReader::Read, Range Overflow 2");

        const char* text = (const char*)_base;
        if (text[size] != 0)
            throw std::exception("PacketReader::ReadString, Bad String");

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
    NOCOPYASSIGN(PacketWriter);
public:
    PacketWriter(int32_t type, int32_t guid, size_t capacity = Packet::DefCapacity)
    {
        _packet = Packet::Create(capacity);
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

        if (_packet->_size < used) {
            if (used > Packet::MaxCapacity)
                throw std::exception("PacketWriter::Write, Exceed MaxSize");    

            PacketPtr packet = Packet::Create(used + Packet::IncCapacity);
            memcpy(&packet->_used, &_packet->_used, _packet->_used + 12);

            _base = (uint8_t*)(packet.Get() + 1) + packet->_used;
            _packet = packet;
        }

        memcpy(_base, data, size);
        _base += size;
        _packet->_used += size;
    }

    void Write(const char* text)
    {
        int length = strlen(text);
        operator<<(length);
        Write(text, length + 1);
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
};

TINYNET_CLOSE()