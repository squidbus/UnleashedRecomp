#include <stdafx.h>
#include "memory.h"

Memory::Memory(void* address, size_t size) : size(size)
{
    base = (char*)VirtualAlloc(address, size, MEM_RESERVE, PAGE_READWRITE);
}

void* Memory::Alloc(size_t offset, size_t size, uint32_t type)
{
    return VirtualAlloc(base + offset, size, type, PAGE_READWRITE);
}

void* Memory::Commit(size_t offset, size_t size)
{
    return Alloc(offset, size, MEM_COMMIT);
}

void* Memory::Reserve(size_t offset, size_t size)
{
    return Alloc(offset, size, MEM_RESERVE);
}

SWA_API void* MmGetHostAddress(uint32_t ptr)
{
    return g_memory.Translate(ptr);
}
