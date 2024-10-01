#include <stdafx.h>
#include "code_cache.h"
#include "ppc_context.h"

CodeCache::CodeCache()
{
    bucket = (char*)VirtualAlloc(nullptr, 0x200000000, MEM_RESERVE, PAGE_READWRITE);
    assert(bucket);
}

CodeCache::~CodeCache()
{
    VirtualFree(bucket, 0, MEM_RELEASE);
}

void CodeCache::Init()
{
    for (size_t i = 0; PPCFuncMappings[i].guest != 0; i++)
    {
        if (PPCFuncMappings[i].host != nullptr)
        {
            VirtualAlloc(bucket + PPCFuncMappings[i].guest * 2, sizeof(void*), MEM_COMMIT, PAGE_READWRITE);
            *(void**)(bucket + PPCFuncMappings[i].guest * 2) = PPCFuncMappings[i].host;
        }
    }
}

void CodeCache::Insert(uint32_t guest, const void* host)
{
    VirtualAlloc(bucket + static_cast<uint64_t>(guest) * 2, sizeof(void*), MEM_COMMIT, PAGE_READWRITE);
    *reinterpret_cast<const void**>(bucket + static_cast<uint64_t>(guest) * 2) = host;
}

void* CodeCache::Find(uint32_t guest) const
{
    return *reinterpret_cast<void**>(bucket + static_cast<uint64_t>(guest) * 2);
}

SWA_API PPCFunc* KeFindHostFunction(uint32_t guest)
{
    return static_cast<PPCFunc*>(g_codeCache.Find(guest));
}

SWA_API void KeInsertHostFunction(uint32_t guest, PPCFunc* function)
{
    g_codeCache.Insert(guest, function);
}
