#include <stdafx.h>
#include "xdm.h"
#include "freelist.h"

FreeList<std::tuple<std::unique_ptr<char>, TypeDestructor_t>> gKernelObjects;
Mutex gKernelLock;

void* ObQueryObject(size_t handle)
{
    std::lock_guard guard{ gKernelLock };

    if (handle >= gKernelObjects.items.size())
        return nullptr;

    return std::get<0>(gKernelObjects[handle]).get();
}

uint32_t ObInsertObject(void* object, TypeDestructor_t destructor)
{
    std::lock_guard guard{ gKernelLock };

    const auto handle = gKernelObjects.Alloc();

    auto& holder = gKernelObjects[handle];

    std::get<0>(holder).reset(static_cast<char*>(object));
    std::get<1>(holder) = destructor;

    return handle;
}

void ObCloseHandle(uint32_t handle)
{
    std::lock_guard guard{ gKernelLock };

    auto& obj = gKernelObjects[handle];

    if (std::get<1>(obj)(std::get<0>(obj).get()))
    {
        std::get<0>(obj).reset();
    }
    else
    {
        std::get<0>(obj).release();
    }

    gKernelObjects.Free(handle);
}
