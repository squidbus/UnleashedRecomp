#pragma once

struct CodeCache
{
    char* bucket{};

    CodeCache();
    ~CodeCache();

    void Init();
    void Insert(uint32_t guest, const void* host);

    void* Find(uint32_t guest) const;
};

extern CodeCache gCodeCache;