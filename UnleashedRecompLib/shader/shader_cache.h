#pragma once

struct ShaderCacheEntry
{
    uint64_t hash;
    uint32_t dxilOffset;
    uint32_t dxilSize;
    uint32_t spirvOffset;
    uint32_t spirvSize;
    void* userData;
};

extern ShaderCacheEntry g_shaderCacheEntries[];
extern size_t g_shaderCacheEntryCount;

extern uint8_t g_compressedDxilCache[];
extern size_t g_dxilCacheCompressedSize;
extern size_t g_dxilCacheDecompressedSize;

extern uint8_t g_compressedSpirvCache[];
extern size_t g_spirvCacheCompressedSize;
extern size_t g_spirvCacheDecompressedSize;
