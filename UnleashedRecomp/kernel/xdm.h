#pragma once
#define DUMMY_HANDLE (DWORD)('HAND')
#define OBJECT_SIGNATURE (DWORD)'XBOX'

extern Mutex gKernelLock;

void* ObQueryObject(size_t handle);
uint32_t ObInsertObject(void* object, TypeDestructor_t destructor);
void ObCloseHandle(uint32_t handle);

template<typename T>
T* ObQueryObject(XDISPATCHER_HEADER& header)
{
    std::lock_guard guard{ gKernelLock };
    if (header.WaitListHead.Flink != OBJECT_SIGNATURE)
    {
        header.WaitListHead.Flink = OBJECT_SIGNATURE;
        auto* obj = new T(reinterpret_cast<typename T::guest_type*>(&header));
        header.WaitListHead.Blink = ObInsertObject(obj, DestroyObject<T>);

        return obj;
    }

    return static_cast<T*>(ObQueryObject(header.WaitListHead.Blink.get()));
}

template<typename T>
size_t ObInsertObject(T* object)
{
    return ObInsertObject(object, DestroyObject<T>);
}

template<typename T>
T* ObCreateObject(int& handle)
{
    auto* obj = new T();
    handle = ::ObInsertObject(obj, DestroyObject<T>);

    return obj;
}

// Get object without initialisation
template<typename T>
T* ObTryQueryObject(XDISPATCHER_HEADER& header)
{
    if (header.WaitListHead.Flink != OBJECT_SIGNATURE)
        return nullptr;

    return static_cast<T*>(ObQueryObject(header.WaitListHead.Blink));
}

template<typename T>
T* ObTryQueryObject(int handle)
{
    return static_cast<T*>(ObQueryObject(handle));
}