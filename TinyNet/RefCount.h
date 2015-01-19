#pragma once
#include "Common.h"

//参考STL智能指针实现，暴露引用计数接口，用来在某些情况延长对象生命周期

NAMESPACE_START(TinyNet)

template<class T>
class RefCount
{
public:
    RefCount(T* ptr) : __ref(1), __ptr(ptr)
    {
    }

    virtual ~RefCount()
    {
    }

    T* Get()
    {
        return __ptr;
    }

    void ForceDelete()
    {
        Destroy();
    }

    uint32_t IncRef()
    {
        return ::InterlockedIncrement(&__ref);
    }

    uint32_t DecRef()
    {
        uint32_t refer = ::InterlockedDecrement(&__ref);
        if (refer == 0) {
            Destroy();
        }
        return refer;
    }
protected:
    T*   __ptr;
private:
    virtual void Destroy() = 0;
    
    volatile uint32_t    __ref;

    NOCOPYASSIGN(RefCount);
};

template<class T>
class RefCount_Default : public RefCount<T>
{
public:
    RefCount_Default(T* ptr) : RefCount(ptr)
    {
    }
private:
    void Destroy()
    {
        delete __ptr;

        delete this;
    }
};

template<class T, class D>
class RefCount_Deleter : public RefCount<T>
{
public:
    RefCount_Deleter(T* ptr, const D& del) : RefCount(ptr), __del(del)
    {
    }
private:
    void Destroy()
    {
        __del(__ptr);

        delete this;
    }

    D    __del;
};

template<class T>
class SharedPtr
{
public:
    SharedPtr(T* ptr = nullptr) : _ptr(ptr),
        _ref(ptr == nullptr ? nullptr : new RefCount_Default<T>(ptr))
    {
    }

    template<class D>
    SharedPtr(T* ptr, const D& del) : _ptr(ptr),
        _ref(ptr == nullptr ? nullptr : new RefCount_Deleter<T, decltype(del)>(ptr, del))
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

template<class T>
RefCount<T>* MakeShared(T* ptr)
{
    return new RefCount_Default<T>(ptr);
}

template<class T, class D>
RefCount<T>* MakeShared(T* ptr, const D& d)
{
    return new RefCount_Deleter(ptr, d);
}

}