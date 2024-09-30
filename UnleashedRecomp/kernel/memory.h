#pragma once

class Memory
{
public:
    char* base{};
    size_t size{};
    size_t guestBase{};

    Memory(void* address, size_t size);

    void* Alloc(size_t offset, size_t size, uint32_t type);

    void* Commit(size_t offset, size_t size);
    void* Reserve(size_t offset, size_t size);

    void* Translate(size_t offset) const noexcept;
    uint32_t MapVirtual(void* host) const noexcept;
};

SWA_API void* MmGetHostAddress(uint32_t ptr);
extern Memory gMemory;
