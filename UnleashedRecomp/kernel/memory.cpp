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

void* Memory::Translate(size_t offset) const noexcept
{
    return base + offset;
}

uint32_t Memory::MapVirtual(void* host) const noexcept
{
    return static_cast<uint32_t>(static_cast<char*>(host) - base);
}

extern "C" void* MmGetHostAddress(uint32_t ptr)
{
    return gMemory.Translate(ptr);
}