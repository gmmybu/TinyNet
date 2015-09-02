#pragma once
#include "Require.h"


TINYNET_START()

template<class T>
class RefCount
{
    NOCOPYASSIGN(RefCount);
public:
    RefCount(T* ptr) : _ref(1), _ptr(ptr)
    {
    }

    virtual ~RefCount()
    {
    }

    T* Get()
    {
        return _ptr;
    }

    void ForceDelete()
    {
        Destroy();
    }

    uint32_t GetRef() const
    {
        return _ref;
    }

    uint32_t IncRef()
    {
        return InterlockedIncrement(&_ref);
    }

    uint32_t DecRef()
    {
        uint32_t ref = InterlockedDecrement(&_ref);
        if (ref == 0) { Destroy(); }
        return ref;
    }
protected:
    virtual void Destroy() = 0;

    T*          _ptr;
    uint32_t    _ref;
};


template<class T>
class RefCount_Default : public RefCount<T>
{
public:
    RefCount_Default(T* ptr) : RefCount(ptr)
    {
    }
protected:
    void Destroy()
    {
        delete _ptr;
        delete this;
    }
};


template<class T, class D>
class RefCount_Deleter : public RefCount<T>
{
public:
    RefCount_Deleter(T* ptr, const D& del) : RefCount(ptr), _del(del)
    {
    }
protected:
    void Destroy()
    {
        _del(_ptr);
        delete this;
    }

    D    _del;
};


template<class T>
RefCount<T>* MakeShared(T* ptr)
{
    return new RefCount_Default<T>(ptr);
}

template<class T, class D>
RefCount<T>* MakeShared(T* ptr, const D& d)
{
    return new RefCount_Deleter<T, decltype(d)>(ptr, d);
}


template<class T>
class SharedPtr
{
public:
    SharedPtr(T* ptr = nullptr) : _ptr(ptr),
        _ref(ptr == nullptr ? nullptr : MakeShared(ptr))
    {
    }

    template<class D>
    SharedPtr(T* ptr, const D& del) : _ptr(ptr),
        _ref(ptr == nullptr ? nullptr : MakeShared(ptr, del))
    {
    }

    SharedPtr(SharedPtr&& rhs)
    {
        _ptr = rhs._ptr;
        _ref = rhs._ref;
        rhs._ptr = nullptr;
        rhs._ref = nullptr;
    }

    SharedPtr(const SharedPtr& rhs)
    {
        _ptr = nullptr;
        _ref = nullptr;
        SetPtr(rhs._ref);
    }

    SharedPtr& operator=(T* ptr)
    {
        if (ptr != _ptr) {
            DelPtr();

            if (ptr != nullptr) {
                _ref = new RefCount_Default<T>(ptr);
                _ptr = ptr;
            }
        }
        return *this;
    }

    SharedPtr& operator=(const SharedPtr& rhs)
    {
        if (_ref != rhs._ref) {
            DelPtr();
            SetPtr(rhs._ref);
        }
        return *this;
    }

    SharedPtr& operator=(SharedPtr&& rhs)
    {
        if (rhs._ptr != _ptr) {
            DelPtr();
            _ptr = rhs._ptr;
            _ref = rhs._ref;
            rhs._ptr = nullptr;
            rhs._ref = nullptr;
        }
        return *this;
    }

    T* Get() const
    {
        return _ptr;
    }

    RefCount<T>* GetRef() const
    {
        return _ref;
    }

    void Reset()
    {
        DelPtr();
    }

    T* operator->() const
    {
        return _ptr;
    }

    T& operator*() const
    {
        return *_ptr;
    }

    ~SharedPtr()
    {
        DelPtr();
    }
private:
    void DelPtr()
    {
        if (_ref != nullptr) {
            _ref->DecRef();
            _ref = nullptr;
            _ptr = nullptr;
        }
    }

    void SetPtr(RefCount<T>* refer)
    {
        if (refer != nullptr) {
            _ptr = refer->Get();
            _ref = refer;
            _ref->IncRef();
        }
    }

    T*             _ptr;
    RefCount<T>*   _ref;
};

TINYNET_CLOSE()