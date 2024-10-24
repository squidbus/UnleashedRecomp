#include <stdafx.h>

#include <kernel/function.h>
#include <kernel/heap.h>
#include <cpu/code_cache.h>
#include <cpu/guest_code.h>
#include <kernel/memory.h>
#include <xxHashMap.h>
#include <shader/shader_cache.h>

#include "gpu/video.h"
#include "ui/window.h"
#include "config.h"

#include "shader/copy_vs.hlsl.dxil.h"
#include "shader/copy_vs.hlsl.spirv.h"
#include "shader/movie_vs.hlsl.dxil.h"
#include "shader/movie_vs.hlsl.spirv.h"
#include "shader/movie_ps.hlsl.dxil.h"
#include "shader/movie_ps.hlsl.spirv.h"
#include "shader/resolve_msaa_depth_2x.hlsl.dxil.h"
#include "shader/resolve_msaa_depth_2x.hlsl.spirv.h"
#include "shader/resolve_msaa_depth_4x.hlsl.dxil.h"
#include "shader/resolve_msaa_depth_4x.hlsl.spirv.h"
#include "shader/resolve_msaa_depth_8x.hlsl.dxil.h"
#include "shader/resolve_msaa_depth_8x.hlsl.spirv.h"

extern "C"
{
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace RT64
{
    extern std::unique_ptr<RenderInterface> CreateD3D12Interface();
    extern std::unique_ptr<RenderInterface> CreateVulkanInterface();
}

struct PipelineState
{
    GuestShader* vertexShader = nullptr;
    GuestShader* pixelShader = nullptr;
    GuestVertexDeclaration* vertexDeclaration = nullptr;
    bool instancing = false;
    bool zEnable = true;
    bool zWriteEnable = true;
    RenderBlend srcBlend = RenderBlend::ONE;
    RenderBlend destBlend = RenderBlend::ZERO;
    RenderCullMode cullMode = RenderCullMode::NONE;
    RenderComparisonFunction zFunc = RenderComparisonFunction::LESS;
    bool alphaBlendEnable = false;
    RenderBlendOperation blendOp = RenderBlendOperation::ADD;
    float slopeScaledDepthBias = 0.0f;
    int32_t depthBias = 0;
    RenderBlend srcBlendAlpha = RenderBlend::ONE;
    RenderBlend destBlendAlpha = RenderBlend::ZERO;
    RenderBlendOperation blendOpAlpha = RenderBlendOperation::ADD;
    uint32_t colorWriteEnable{};
    RenderPrimitiveTopology primitiveTopology = RenderPrimitiveTopology::TRIANGLE_LIST;
    uint8_t vertexStrides[16]{};
    RenderFormat renderTargetFormat{};
    RenderFormat depthStencilFormat{};
    RenderSampleCounts sampleCount = RenderSampleCount::COUNT_1;
    bool enableAlphaToCoverage = false;
};

enum class AlphaTestMode : uint32_t
{
    Disabled,
    AlphaThreshold,
    AlphaToCoverage
};

struct SharedConstants
{
    uint32_t textureIndices[16]{};
    uint32_t samplerIndices[16]{};
    AlphaTestMode alphaTestMode{};
    float alphaThreshold{};
    uint32_t booleans{};
    uint32_t swappedTexcoords{};
    uint32_t inputLayoutFlags{};
    uint32_t enableGIBicubicFiltering{};
};

static GuestSurface* g_renderTarget;
static GuestSurface* g_depthStencil;
static RenderViewport g_viewport(0.0f, 0.0f, 1280.0f, 720.0f);
static bool g_halfPixel = true;
static PipelineState g_pipelineState;
static SharedConstants g_sharedConstants;
static RenderSamplerDesc g_samplerDescs[16];
static bool g_scissorTestEnable = false;
static RenderRect g_scissorRect;
static RenderVertexBufferView g_vertexBufferViews[16];
static RenderInputSlot g_inputSlots[16];
static RenderIndexBufferView g_indexBufferView({}, 0, RenderFormat::R16_UINT);

struct DirtyStates
{
    bool renderTargetAndDepthStencil;
    bool viewport;
    bool pipelineState;
    bool sharedConstants;
    bool scissorRect;
    bool vertexShaderConstants;
    uint8_t vertexStreamFirst;
    uint8_t vertexStreamLast;
    bool indices;
    bool pixelShaderConstants;

    DirtyStates(bool value)
        : renderTargetAndDepthStencil(value)
        , viewport(value)
        , pipelineState(value)
        , sharedConstants(value)
        , scissorRect(value)
        , vertexShaderConstants(value)
        , vertexStreamFirst(value ? 0 : 255)
        , vertexStreamLast(value ? 15 : 0)
        , indices(value)
        , pixelShaderConstants(value)
    {
    }
};

static DirtyStates g_dirtyStates(true);

template<typename T>
static FORCEINLINE void SetDirtyValue(bool& dirtyState, T& dest, const T& src)
{
    if (dest != src)
    {
        dest = src;
        dirtyState = true;
    }
}

static bool g_vulkan;
static std::unique_ptr<RenderInterface> g_interface;
static std::unique_ptr<RenderDevice> g_device;

static constexpr size_t NUM_FRAMES = 2;

static uint32_t g_frame = 0;
static uint32_t g_nextFrame = 1;

static std::unique_ptr<RenderCommandQueue> g_queue;
static std::unique_ptr<RenderCommandList> g_commandLists[NUM_FRAMES];
static std::unique_ptr<RenderCommandFence> g_commandFences[NUM_FRAMES];
static bool g_commandListStates[NUM_FRAMES];

static Mutex g_copyMutex;
static std::unique_ptr<RenderCommandQueue> g_copyQueue;
static std::unique_ptr<RenderCommandList> g_copyCommandList;
static std::unique_ptr<RenderCommandFence> g_copyCommandFence;

static std::unique_ptr<RenderSwapChain> g_swapChain;
static bool g_swapChainValid;

static std::unique_ptr<RenderCommandSemaphore> g_acquireSemaphores[NUM_FRAMES];
static std::unique_ptr<RenderCommandSemaphore> g_renderSemaphores[NUM_FRAMES];
static uint32_t g_backBufferIndex;
static GuestSurface* g_backBuffer;

struct std::unique_ptr<RenderDescriptorSet> g_textureDescriptorSet;
struct std::unique_ptr<RenderDescriptorSet> g_samplerDescriptorSet;

struct TextureDescriptorAllocator
{
    Mutex mutex;
    uint32_t capacity = 0;
    std::vector<uint32_t> freed;

    uint32_t allocate()
    {
        std::lock_guard lock(mutex);

        uint32_t value;
        if (!freed.empty())
        {
            value = freed.back();
            freed.pop_back();
        }
        else
        {
            value = ++capacity;
        }

        return value;
    }

    void free(uint32_t value)
    {
        assert(value != NULL);
        std::lock_guard lock(mutex);
        freed.push_back(value);
    }
};

static TextureDescriptorAllocator g_textureDescriptorAllocator;

static std::unique_ptr<RenderPipelineLayout> g_pipelineLayout;
static xxHashMap<std::unique_ptr<RenderPipeline>> g_pipelines;

static xxHashMap<std::pair<uint32_t, std::unique_ptr<RenderSampler>>> g_samplerStates;

static Mutex g_vertexDeclarationMutex;
static xxHashMap<GuestVertexDeclaration*> g_vertexDeclarations;

struct UploadBuffer
{
    static constexpr size_t SIZE = 16 * 1024 * 1024;

    std::unique_ptr<RenderBuffer> buffer;
    uint8_t* memory = nullptr;
    uint64_t deviceAddress = 0;
};

struct UploadAllocation
{
    RenderBufferReference bufferReference;
    uint8_t* memory = nullptr;
    uint64_t deviceAddress = 0;
};

struct UploadAllocator
{
    std::vector<UploadBuffer> buffers;
    uint32_t index = 0;
    uint32_t offset = 0;

    UploadAllocation allocate(uint32_t size, uint32_t alignment)
    {
        assert(size <= UploadBuffer::SIZE);

        offset = (offset + alignment - 1) & ~(alignment - 1);

        if (offset + size > UploadBuffer::SIZE)
        {
            ++index;
            offset = 0;
        }

        if (buffers.size() <= index)
            buffers.resize(index + 1);

        auto& buffer = buffers[index];
        if (buffer.buffer == nullptr)
        {
            buffer.buffer = g_device->createBuffer(RenderBufferDesc::UploadBuffer(UploadBuffer::SIZE, RenderBufferFlag::CONSTANT | RenderBufferFlag::VERTEX | RenderBufferFlag::INDEX));
            buffer.memory = reinterpret_cast<uint8_t*>(buffer.buffer->map());
            buffer.deviceAddress = buffer.buffer->getDeviceAddress();
        }
        
        auto ref = buffer.buffer->at(offset);
        offset += size;

        return { ref, buffer.memory + ref.offset, buffer.deviceAddress + ref.offset };
    }

    template<bool TByteSwap, typename T>
    UploadAllocation allocate(const T* memory, uint32_t size, uint32_t alignment)
    {
        auto result = allocate(size, alignment);

        if constexpr (TByteSwap)
        {
            auto destination = reinterpret_cast<T*>(result.memory);

            for (size_t i = 0; i < size; i += sizeof(T))
            {
                *destination = std::byteswap(*memory);
                ++destination;
                ++memory;
            }
        }
        else
        {
            memcpy(result.memory, memory, size);
        }

        return result;
    }

    void reset()
    {
        index = 0;
        offset = 0;
    }
};

static UploadAllocator g_uploadAllocators[NUM_FRAMES];

static Mutex g_tempMutex;
static std::vector<std::unique_ptr<RenderTexture>> g_tempTextures[NUM_FRAMES];
static std::vector<std::unique_ptr<RenderBuffer>> g_tempBuffers[NUM_FRAMES];
static std::vector<uint32_t> g_tempDescriptorIndices[NUM_FRAMES];

static RenderBufferReference g_quadIndexBuffer;
static uint32_t g_quadCount;

static uint32_t g_mainThreadId;

static ankerl::unordered_dense::map<RenderTexture*, RenderTextureLayout> g_barrierMap;

static void AddBarrier(GuestBaseTexture* texture, RenderTextureLayout layout)
{
    if (texture != nullptr && texture->layout != layout)
    {
        g_barrierMap[texture->texture] = layout;
        texture->layout = layout;
    }
}

static std::vector<RenderTextureBarrier> g_barriers;

static void FlushBarriers()
{
    if (!g_barrierMap.empty())
    {
        for (auto& [texture, layout] : g_barrierMap)
            g_barriers.emplace_back(texture, layout);

        g_commandLists[g_frame]->barriers(RenderBarrierStage::GRAPHICS | RenderBarrierStage::COPY, g_barriers);

        g_barrierMap.clear();
        g_barriers.clear();
    }
}

static std::unique_ptr<uint8_t[]> g_shaderCache;

static void LoadShaderCache()
{
    const size_t decompressedSize = g_vulkan ? g_spirvCacheDecompressedSize : g_dxilCacheDecompressedSize;
    g_shaderCache = std::make_unique<uint8_t[]>(decompressedSize);

    ZSTD_decompress(g_shaderCache.get(), 
        decompressedSize, 
        g_vulkan ? g_compressedSpirvCache : g_compressedDxilCache, 
        g_vulkan ? g_spirvCacheCompressedSize : g_dxilCacheCompressedSize);
}

static void SetRenderState(GuestDevice* device, uint32_t value)
{
}

static void SetRenderStateZEnable(GuestDevice* device, uint32_t value)
{
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.zEnable, value != 0);
}

static void SetRenderStateZWriteEnable(GuestDevice* device, uint32_t value)
{
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.zWriteEnable, value != 0);
}

static void SetAlphaTestMode(bool enable)
{
    AlphaTestMode alphaTestMode = AlphaTestMode::Disabled;

    if (enable)
    {
        if (Config::AlphaToCoverage && g_renderTarget != nullptr && g_renderTarget->sampleCount != RenderSampleCount::COUNT_1)
            alphaTestMode = AlphaTestMode::AlphaToCoverage;
        else
            alphaTestMode = AlphaTestMode::AlphaThreshold;
    }

    SetDirtyValue(g_dirtyStates.sharedConstants, g_sharedConstants.alphaTestMode, alphaTestMode);
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.enableAlphaToCoverage, alphaTestMode == AlphaTestMode::AlphaToCoverage);
}

static void SetRenderStateAlphaTestEnable(GuestDevice* device, uint32_t value)
{
    SetAlphaTestMode(value != 0);
}

static RenderBlend ConvertBlendMode(uint32_t blendMode)
{
    switch (blendMode)
    {
    case D3DBLEND_ZERO: 
        return RenderBlend::ZERO;
    case D3DBLEND_ONE:
        return RenderBlend::ONE;
    case D3DBLEND_SRCCOLOR:
        return RenderBlend::SRC_COLOR;
    case D3DBLEND_INVSRCCOLOR:
        return RenderBlend::INV_SRC_COLOR;
    case D3DBLEND_SRCALPHA: 
        return RenderBlend::SRC_ALPHA;
    case D3DBLEND_INVSRCALPHA: 
        return RenderBlend::INV_SRC_ALPHA;
    case D3DBLEND_DESTCOLOR: 
        return RenderBlend::DEST_COLOR;
    case D3DBLEND_INVDESTCOLOR: 
        return RenderBlend::INV_DEST_COLOR;
    case D3DBLEND_DESTALPHA: 
        return RenderBlend::DEST_ALPHA;
    case D3DBLEND_INVDESTALPHA: 
        return RenderBlend::INV_DEST_ALPHA;
    default:
        assert(false && "Invalid blend mode");
        return RenderBlend::ZERO;
    }
}

static void SetRenderStateSrcBlend(GuestDevice* device, uint32_t value)
{
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.srcBlend, ConvertBlendMode(value));
}

static void SetRenderStateDestBlend(GuestDevice* device, uint32_t value)
{
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.destBlend, ConvertBlendMode(value));
}

static void SetRenderStateCullMode(GuestDevice* device, uint32_t value)
{
    RenderCullMode cullMode;

    switch (value) {
    case D3DCULL_NONE:
    case D3DCULL_NONE_2:
        cullMode = RenderCullMode::NONE;
        break;
    case D3DCULL_CW:
        cullMode = RenderCullMode::FRONT;
        break;
    case D3DCULL_CCW:
        cullMode = RenderCullMode::BACK;
        break;
    default:
        assert(false && "Invalid cull mode");
        cullMode = RenderCullMode::NONE;
        break;
    }

    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.cullMode, cullMode);
}

static void SetRenderStateZFunc(GuestDevice* device, uint32_t value)
{
    RenderComparisonFunction comparisonFunc;

    switch (value)
    {
    case D3DCMP_NEVER:
        comparisonFunc = RenderComparisonFunction::NEVER;
        break;
    case D3DCMP_LESS:
        comparisonFunc = RenderComparisonFunction::LESS;
        break;
    case D3DCMP_EQUAL:
        comparisonFunc = RenderComparisonFunction::EQUAL;
        break;
    case D3DCMP_LESSEQUAL:
        comparisonFunc = RenderComparisonFunction::LESS_EQUAL;
        break;
    case D3DCMP_GREATER:
        comparisonFunc = RenderComparisonFunction::GREATER;
        break;
    case D3DCMP_NOTEQUAL:
        comparisonFunc = RenderComparisonFunction::NOT_EQUAL;
        break;
    case D3DCMP_GREATEREQUAL:
        comparisonFunc = RenderComparisonFunction::GREATER_EQUAL;
        break;
    case D3DCMP_ALWAYS:
        comparisonFunc = RenderComparisonFunction::ALWAYS;
        break;
    default:
        assert(false && "Unknown comparison function");
        comparisonFunc = RenderComparisonFunction::NEVER;
        break;
    }

    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.zFunc, comparisonFunc);
}

static void SetRenderStateAlphaRef(GuestDevice* device, uint32_t value)
{
    SetDirtyValue(g_dirtyStates.pipelineState, g_sharedConstants.alphaThreshold, float(value) / 256.0f);
}

static void SetRenderStateAlphaBlendEnable(GuestDevice* device, uint32_t value)
{
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.alphaBlendEnable, value != 0);
}

static RenderBlendOperation ConvertBlendOp(uint32_t blendOp)
{
    switch (blendOp)
    {
    case D3DBLENDOP_ADD:
        return RenderBlendOperation::ADD;
    case D3DBLENDOP_SUBTRACT:
        return RenderBlendOperation::SUBTRACT;
    case D3DBLENDOP_REVSUBTRACT:
        return RenderBlendOperation::REV_SUBTRACT;
    case D3DBLENDOP_MIN:
        return RenderBlendOperation::MIN;
    case D3DBLENDOP_MAX:
        return RenderBlendOperation::MAX;
    default:
        assert(false && "Unknown blend operation");
        return RenderBlendOperation::ADD;
    }
}

static void SetRenderStateBlendOp(GuestDevice* device, uint32_t value)
{
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.blendOp, ConvertBlendOp(value));
}

static void SetRenderStateScissorTestEnable(GuestDevice* device, uint32_t value)
{
    SetDirtyValue(g_dirtyStates.scissorRect, g_scissorTestEnable, value != 0);
}

static void SetRenderStateSlopeScaledDepthBias(GuestDevice* device, uint32_t value)
{
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.slopeScaledDepthBias, *reinterpret_cast<float*>(&value));
}

static void SetRenderStateDepthBias(GuestDevice* device, uint32_t value)
{
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.depthBias, int32_t(*reinterpret_cast<float*>(&value) * (1 << 24)));
}

static void SetRenderStateSrcBlendAlpha(GuestDevice* device, uint32_t value)
{
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.srcBlendAlpha, ConvertBlendMode(value));
}

static void SetRenderStateDestBlendAlpha(GuestDevice* device, uint32_t value)
{
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.destBlendAlpha, ConvertBlendMode(value));
}

static void SetRenderStateBlendOpAlpha(GuestDevice* device, uint32_t value)
{
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.blendOpAlpha, ConvertBlendOp(value));
}

static void SetRenderStateColorWriteEnable(GuestDevice* device, uint32_t value)
{
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.colorWriteEnable, value);
}

static const std::pair<GuestRenderState, void*> g_setRenderStateFunctions[] =
{
    { D3DRS_ZENABLE, GuestFunction<SetRenderStateZEnable> },
    { D3DRS_ZWRITEENABLE, GuestFunction<SetRenderStateZWriteEnable> },
    { D3DRS_ALPHATESTENABLE, GuestFunction<SetRenderStateAlphaTestEnable> },
    { D3DRS_SRCBLEND, GuestFunction<SetRenderStateSrcBlend> },
    { D3DRS_DESTBLEND, GuestFunction<SetRenderStateDestBlend> },
    { D3DRS_CULLMODE, GuestFunction<SetRenderStateCullMode> },
    { D3DRS_ZFUNC, GuestFunction<SetRenderStateZFunc> },
    { D3DRS_ALPHAREF, GuestFunction<SetRenderStateAlphaRef> },
    { D3DRS_ALPHABLENDENABLE, GuestFunction<SetRenderStateAlphaBlendEnable> },
    { D3DRS_BLENDOP, GuestFunction<SetRenderStateBlendOp> },
    { D3DRS_SCISSORTESTENABLE, GuestFunction<SetRenderStateScissorTestEnable> },
    { D3DRS_SLOPESCALEDEPTHBIAS, GuestFunction<SetRenderStateSlopeScaledDepthBias> },
    { D3DRS_DEPTHBIAS, GuestFunction<SetRenderStateDepthBias> },
    { D3DRS_SRCBLENDALPHA, GuestFunction<SetRenderStateSrcBlendAlpha> },
    { D3DRS_DESTBLENDALPHA, GuestFunction<SetRenderStateDestBlendAlpha> },
    { D3DRS_BLENDOPALPHA, GuestFunction<SetRenderStateBlendOpAlpha> },
    { D3DRS_COLORWRITEENABLE, GuestFunction<SetRenderStateColorWriteEnable> }
};

static std::unique_ptr<RenderPipeline> g_resolveMsaaDepthPipelines[3];

#define CREATE_SHADER(NAME) \
    g_device->createShader( \
        g_vulkan ? g_##NAME##_spirv : g_##NAME##_dxil, \
        g_vulkan ? sizeof(g_##NAME##_spirv) : sizeof(g_##NAME##_dxil), \
        "main", \
        g_vulkan ? RenderShaderFormat::SPIRV : RenderShaderFormat::DXIL)

static bool DetectWine()
{
    HMODULE dllHandle = GetModuleHandle("ntdll.dll");
    return dllHandle != nullptr && GetProcAddress(dllHandle, "wine_get_version") != nullptr;
}

static void CreateHostDevice()
{
    for (uint32_t i = 0; i < 16; i++)
        g_inputSlots[i].index = i;

    Window::Init();

    g_vulkan = DetectWine() || Config::GraphicsAPI == EGraphicsAPI::Vulkan;

    LoadShaderCache();

    g_interface = g_vulkan ? CreateVulkanInterface() : CreateD3D12Interface();
    g_device = g_interface->createDevice();

    g_queue = g_device->createCommandQueue(RenderCommandListType::DIRECT);

    for (auto& commandList : g_commandLists)
        commandList = g_device->createCommandList(RenderCommandListType::DIRECT);

    for (auto& commandFence : g_commandFences)
        commandFence = g_device->createCommandFence();

    g_copyQueue = g_device->createCommandQueue(RenderCommandListType::COPY);
    g_copyCommandList = g_device->createCommandList(RenderCommandListType::COPY);
    g_copyCommandFence = g_device->createCommandFence();

    g_swapChain = g_queue->createSwapChain(Window::s_windowHandle, Config::TripleBuffering ? 3 : 2, RenderFormat::B8G8R8A8_UNORM);

    for (auto& acquireSemaphore : g_acquireSemaphores)
        acquireSemaphore = g_device->createCommandSemaphore();
    
    for (auto& renderSemaphore : g_renderSemaphores)
        renderSemaphore = g_device->createCommandSemaphore();

    g_mainThreadId = GetCurrentThreadId();

    RenderPipelineLayoutBuilder pipelineLayoutBuilder;
    pipelineLayoutBuilder.begin(false, true);

    constexpr size_t TEXTURE_DESCRIPTOR_SIZE = 65536;
    constexpr size_t SAMPLER_DESCRIPTOR_SIZE = 1024;
    
    RenderDescriptorSetBuilder descriptorSetBuilder;
    descriptorSetBuilder.begin();
    descriptorSetBuilder.addTexture(0, TEXTURE_DESCRIPTOR_SIZE);
    descriptorSetBuilder.end(true, TEXTURE_DESCRIPTOR_SIZE);
    
    g_textureDescriptorSet = descriptorSetBuilder.create(g_device.get());
    g_textureDescriptorSet->setTexture(0, nullptr, RenderTextureLayout::SHADER_READ);

    pipelineLayoutBuilder.addDescriptorSet(descriptorSetBuilder);
    pipelineLayoutBuilder.addDescriptorSet(descriptorSetBuilder);
    pipelineLayoutBuilder.addDescriptorSet(descriptorSetBuilder);
    
    descriptorSetBuilder.begin();
    descriptorSetBuilder.addSampler(0, SAMPLER_DESCRIPTOR_SIZE);
    descriptorSetBuilder.end(true, SAMPLER_DESCRIPTOR_SIZE);
    
    g_samplerDescriptorSet = descriptorSetBuilder.create(g_device.get());
    auto& [descriptorIndex, sampler] = g_samplerStates[XXH3_64bits(&g_samplerDescs[0], sizeof(RenderSamplerDesc))];
    descriptorIndex = 1;
    sampler = g_device->createSampler(g_samplerDescs[0]);
    g_samplerDescriptorSet->setSampler(0, sampler.get());

    pipelineLayoutBuilder.addDescriptorSet(descriptorSetBuilder);

    if (g_vulkan)
    {
        pipelineLayoutBuilder.addPushConstant(0, 4, 24, RenderShaderStageFlag::VERTEX | RenderShaderStageFlag::PIXEL);
    }
    else
    {
        pipelineLayoutBuilder.addRootDescriptor(0, 4, RenderRootDescriptorType::CONSTANT_BUFFER);
        pipelineLayoutBuilder.addRootDescriptor(1, 4, RenderRootDescriptorType::CONSTANT_BUFFER);
        pipelineLayoutBuilder.addRootDescriptor(2, 4, RenderRootDescriptorType::CONSTANT_BUFFER);
        pipelineLayoutBuilder.addPushConstant(3, 4, 4, RenderShaderStageFlag::PIXEL); // For copy/resolve shaders.
    }
    pipelineLayoutBuilder.end();
    
    g_pipelineLayout = pipelineLayoutBuilder.create(g_device.get());

    auto copyShader = CREATE_SHADER(copy_vs);

    for (size_t i = 0; i < std::size(g_resolveMsaaDepthPipelines); i++)
    {
        std::unique_ptr<RenderShader> pixelShader;
        switch (i)
        {
        case 0:
            pixelShader = CREATE_SHADER(resolve_msaa_depth_2x);
            break;
        case 1:
            pixelShader = CREATE_SHADER(resolve_msaa_depth_4x);
            break;
        case 2:
            pixelShader = CREATE_SHADER(resolve_msaa_depth_8x);
            break;
        }

        RenderGraphicsPipelineDesc desc;
        desc.pipelineLayout = g_pipelineLayout.get();
        desc.vertexShader = copyShader.get();
        desc.pixelShader = pixelShader.get();
        desc.depthFunction = RenderComparisonFunction::ALWAYS;
        desc.depthEnabled = true;
        desc.depthWriteEnabled = true;
        desc.depthTargetFormat = RenderFormat::D32_FLOAT;
        g_resolveMsaaDepthPipelines[i] = g_device->createGraphicsPipeline(desc);
    }
}

static void WaitForGPU()
{
    if (g_vulkan)
    {
        g_device->waitIdle();
    }
    else 
    {
        for (size_t i = 0; i < NUM_FRAMES; i++)
        {
            if (g_commandListStates[i])
            {
                g_queue->waitForCommandFence(g_commandFences[i].get());
                g_commandListStates[i] = false;
            }
        }
        g_queue->executeCommandLists(nullptr, g_commandFences[0].get());
        g_queue->waitForCommandFence(g_commandFences[0].get());
    }
}

static PPCRegister g_r3;
static PPCRegister g_r4;
static PPCRegister g_r5;

PPC_FUNC_IMPL(__imp__sub_8258C8A0);
PPC_FUNC(sub_8258C8A0)
{
    g_r3 = ctx.r3;
    g_r4 = ctx.r4;
    g_r5 = ctx.r5;
    __imp__sub_8258C8A0(ctx, base);
}

static void ResizeSwapChain()
{
    WaitForGPU();
    g_backBuffer->framebuffers.clear();

    if (g_swapChain->resize() && g_r3.u32 != NULL)
    {
        auto ctx = GetPPCContext();
        ctx->r3 = g_r3;
        ctx->r4 = g_r4;
        ctx->r5 = g_r5;
        GuestCode::Run(__imp__sub_8258C8A0, ctx);
    }
}

static void BeginCommandList()
{
    g_renderTarget = g_backBuffer;
    g_depthStencil = nullptr;

    g_pipelineState.renderTargetFormat = g_backBuffer->format;
    g_pipelineState.depthStencilFormat = RenderFormat::UNKNOWN;

    g_swapChainValid = !g_swapChain->needsResize();
    if (g_swapChainValid)
    {
        g_swapChainValid = g_swapChain->acquireTexture(g_acquireSemaphores[g_frame].get(), &g_backBufferIndex);
        if (g_swapChainValid)
            g_backBuffer->texture = g_swapChain->getTexture(g_backBufferIndex);
    }
    else
    {
        ResizeSwapChain();
    }

    if (!g_swapChainValid)
        g_backBuffer->texture = g_backBuffer->textureHolder.get();

    g_backBuffer->layout = RenderTextureLayout::UNKNOWN;

    g_sharedConstants.enableGIBicubicFiltering = (Config::GITextureFiltering == EGITextureFiltering::Bicubic);

    auto& commandList = g_commandLists[g_frame];

    commandList->begin();
    commandList->setGraphicsPipelineLayout(g_pipelineLayout.get());
    commandList->setGraphicsDescriptorSet(g_textureDescriptorSet.get(), 0);
    commandList->setGraphicsDescriptorSet(g_textureDescriptorSet.get(), 1);
    commandList->setGraphicsDescriptorSet(g_textureDescriptorSet.get(), 2);
    commandList->setGraphicsDescriptorSet(g_samplerDescriptorSet.get(), 3);
}

static uint32_t CreateDevice(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, be<uint32_t>* a6)
{
    CreateHostDevice();

    g_backBuffer = g_userHeap.AllocPhysical<GuestSurface>(ResourceType::RenderTarget);
    g_backBuffer->width = 1280;
    g_backBuffer->height = 720;
    g_backBuffer->format = RenderFormat::B8G8R8A8_UNORM;
    g_backBuffer->textureHolder = g_device->createTexture(RenderTextureDesc::Texture2D(16, 16, 1, g_backBuffer->format, RenderTextureFlag::RENDER_TARGET));

    BeginCommandList();

    auto device = g_userHeap.AllocPhysical<GuestDevice>();
    memset(device, 0, sizeof(*device));

    uint32_t functionOffset = 'D3D';
    g_codeCache.Insert(functionOffset, reinterpret_cast<void*>(GuestFunction<SetRenderState>));

    for (size_t i = 0; i < _countof(device->setRenderStateFunctions); i++)
        device->setRenderStateFunctions[i] = functionOffset;

    for (auto& [state, function] : g_setRenderStateFunctions)
    {
        functionOffset += 4;
        g_codeCache.Insert(functionOffset, function);
        device->setRenderStateFunctions[state / 4] = functionOffset;
    }

    for (size_t i = 0; i < _countof(device->setSamplerStateFunctions); i++)
        device->setSamplerStateFunctions[i] = *reinterpret_cast<uint32_t*>(g_memory.Translate(0x8330F3DC + i * 0xC));

    *a6 = g_memory.MapVirtual(device);

    return 0;
}

static void DestructResource(GuestResource* resource) 
{
    switch (resource->type)
    {
    case ResourceType::Texture:
    case ResourceType::VolumeTexture:
    {
        const auto texture = reinterpret_cast<GuestTexture*>(resource);

        if (texture->mappedMemory != nullptr)
            g_userHeap.Free(texture->mappedMemory);

        {
            std::lock_guard lock(g_tempMutex);
            g_tempTextures[g_frame].emplace_back(std::move(texture->textureHolder));
            g_tempDescriptorIndices[g_frame].push_back(texture->descriptorIndex);
        }

        texture->~GuestTexture();
        break;
    }
           
    case ResourceType::VertexBuffer:
    case ResourceType::IndexBuffer:
    {
        const auto buffer = reinterpret_cast<GuestBuffer*>(resource);

        if (buffer->mappedMemory != nullptr)
            g_userHeap.Free(buffer->mappedMemory);

        {
            std::lock_guard lock(g_tempMutex);
            g_tempBuffers[g_frame].emplace_back(std::move(buffer->buffer));
        }

        buffer->~GuestBuffer();
        break;
    }
            
    case ResourceType::RenderTarget:
    case ResourceType::DepthStencil:
    {
        const auto surface = reinterpret_cast<GuestSurface*>(resource);

        {
            std::lock_guard lock(g_tempMutex);
            g_tempTextures[g_frame].emplace_back(std::move(surface->textureHolder));

            if (surface->descriptorIndex != NULL)
                g_tempDescriptorIndices[g_frame].push_back(surface->descriptorIndex);
        }

        surface->~GuestSurface();
        break;
    }
            
    case ResourceType::VertexDeclaration:
        reinterpret_cast<GuestVertexDeclaration*>(resource)->~GuestVertexDeclaration();
        break;
            
    case ResourceType::VertexShader:
    case ResourceType::PixelShader:
        reinterpret_cast<GuestShader*>(resource)->~GuestShader();
        break;
    }

    g_userHeap.Free(resource);
}

static constexpr uint32_t PITCH_ALIGNMENT = 0x100;
static constexpr uint32_t PLACEMENT_ALIGNMENT = 0x200;

static uint32_t ComputeTexturePitch(GuestTexture* texture)
{
    return (texture->width * RenderFormatSize(texture->format) + PITCH_ALIGNMENT - 1) & ~(PITCH_ALIGNMENT - 1);
}

static void LockTextureRect(GuestTexture* texture, uint32_t, GuestLockedRect* lockedRect) 
{
    uint32_t pitch = ComputeTexturePitch(texture);
    uint32_t slicePitch = pitch * texture->height;

    if (texture->mappedMemory == nullptr)
        texture->mappedMemory = g_userHeap.AllocPhysical(slicePitch, 0x10);

    lockedRect->pitch = pitch;
    lockedRect->bits = g_memory.MapVirtual(texture->mappedMemory);
}

static void UnlockTextureRect(GuestTexture* texture) 
{
    assert(GetCurrentThreadId() == g_mainThreadId);

    AddBarrier(texture, RenderTextureLayout::COPY_DEST);
    FlushBarriers();

    uint32_t pitch = ComputeTexturePitch(texture);
    uint32_t slicePitch = pitch * texture->height;

    auto allocation = g_uploadAllocators[g_frame].allocate(slicePitch, PLACEMENT_ALIGNMENT);
    memcpy(allocation.memory, texture->mappedMemory, slicePitch);
    
    g_commandLists[g_frame]->copyTextureRegion(
        RenderTextureCopyLocation::Subresource(texture->texture, 0),
        RenderTextureCopyLocation::PlacedFootprint(allocation.bufferReference.ref, texture->format, texture->width, texture->height, 1, pitch / RenderFormatSize(texture->format), allocation.bufferReference.offset));
}

static void* LockBuffer(GuestBuffer* buffer, uint32_t flags)
{
    buffer->lockedReadOnly = (flags & 0x10) != 0;

    if (buffer->mappedMemory == nullptr)
        buffer->mappedMemory = g_userHeap.AllocPhysical(buffer->dataSize, 0x10);

    return buffer->mappedMemory;
}

static void* LockVertexBuffer(GuestBuffer* buffer, uint32_t, uint32_t, uint32_t flags)
{
    return LockBuffer(buffer, flags);
}

template<typename T>
static void ExecuteCopyCommandList(const T& function)
{
    std::lock_guard lock(g_copyMutex);

    g_copyCommandList->begin();
    function();
    g_copyCommandList->end();
    g_copyQueue->executeCommandLists(g_copyCommandList.get(), g_copyCommandFence.get());
    g_copyQueue->waitForCommandFence(g_copyCommandFence.get());
}

template<typename T>
static void UnlockBuffer(GuestBuffer* buffer)
{
    if (!buffer->lockedReadOnly)
    {
        auto uploadBuffer = g_device->createBuffer(RenderBufferDesc::UploadBuffer(buffer->dataSize));
        
        auto dest = reinterpret_cast<T*>(uploadBuffer->map());
        auto src = reinterpret_cast<const T*>(buffer->mappedMemory);

        for (size_t i = 0; i < buffer->dataSize; i += sizeof(T))
        {
            *dest = std::byteswap(*src);
            ++dest;
            ++src;
        }

        uploadBuffer->unmap();

        ExecuteCopyCommandList([&]
            {
                g_copyCommandList->copyBufferRegion(buffer->buffer->at(0), uploadBuffer->at(0), buffer->dataSize);
            });
    }
}

static void UnlockVertexBuffer(GuestBuffer* buffer)
{
    UnlockBuffer<uint32_t>(buffer);
}

static void GetVertexBufferDesc(GuestBuffer* buffer, GuestBufferDesc* desc) 
{
    desc->size = buffer->dataSize;
}

static void* LockIndexBuffer(GuestBuffer* buffer, uint32_t, uint32_t, uint32_t flags) 
{
    return LockBuffer(buffer, flags);
}

static void UnlockIndexBuffer(GuestBuffer* buffer) 
{
    UnlockBuffer<uint16_t>(buffer);
}

static void GetIndexBufferDesc(GuestBuffer* buffer, GuestBufferDesc* desc)
{
    desc->format = buffer->guestFormat;
    desc->size = buffer->dataSize;
}

static void GetSurfaceDesc(GuestSurface* surface, GuestSurfaceDesc* desc) 
{
    desc->width = surface->width;
    desc->height = surface->height;
}

static void GetVertexDeclaration(GuestVertexDeclaration* vertexDeclaration, GuestVertexElement* vertexElements, be<uint32_t>* count) 
{
    memcpy(vertexElements, vertexDeclaration->vertexElements.get(), vertexDeclaration->vertexElementCount * sizeof(GuestVertexElement));
    *count = vertexDeclaration->vertexElementCount;
}

static uint32_t HashVertexDeclaration(uint32_t vertexDeclaration) 
{
    // Vertex declarations are cached on host side, so the pointer itself can be used.
    return vertexDeclaration;
}

static void Present() 
{
    if (g_swapChainValid)
        AddBarrier(g_backBuffer, RenderTextureLayout::PRESENT);
    
    FlushBarriers();

    auto& commandList = g_commandLists[g_frame];
    commandList->end();

    if (g_swapChainValid)
    {
        const RenderCommandList* commandLists[] = { commandList.get() };
        RenderCommandSemaphore* waitSemaphores[] = { g_acquireSemaphores[g_frame].get() };
        RenderCommandSemaphore* signalSemaphores[] = { g_renderSemaphores[g_frame].get() };

        g_queue->executeCommandLists(
            commandLists, std::size(commandLists),
            waitSemaphores, std::size(waitSemaphores),
            signalSemaphores, std::size(signalSemaphores),
            g_commandFences[g_frame].get());

        g_swapChainValid = g_swapChain->present(g_backBufferIndex, signalSemaphores, std::size(signalSemaphores));
    }
    else
    {
        g_queue->executeCommandLists(commandList.get(), g_commandFences[g_frame].get());
    }

    g_commandListStates[g_frame] = true;

    g_frame = g_nextFrame;
    g_nextFrame = (g_frame + 1) % NUM_FRAMES;

    if (g_commandListStates[g_frame])
    {
        g_queue->waitForCommandFence(g_commandFences[g_frame].get());
        g_commandListStates[g_frame] = false;
    }

    {
        std::lock_guard lock(g_tempMutex);

        g_tempBuffers[g_frame].clear();
        g_tempTextures[g_frame].clear();

        for (auto index : g_tempDescriptorIndices[g_frame])
            g_textureDescriptorAllocator.free(index);

        g_tempDescriptorIndices[g_frame].clear();
    }

    g_dirtyStates = DirtyStates(true);
    g_uploadAllocators[g_frame].reset();
    g_quadIndexBuffer = {};
    g_quadCount = 0;

    BeginCommandList();
}

static GuestSurface* GetBackBuffer() 
{
    g_backBuffer->AddRef();
    return g_backBuffer;
}

static RenderFormat ConvertFormat(uint32_t format)
{
    switch (format)
    {
    case D3DFMT_A16B16G16R16F:
    case D3DFMT_A16B16G16R16F_2:
        return RenderFormat::R16G16B16A16_FLOAT;
    case D3DFMT_A8B8G8R8:
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8:
        return RenderFormat::R8G8B8A8_UNORM;
    case D3DFMT_D24FS8:
    case D3DFMT_D24S8:
        return RenderFormat::D32_FLOAT;
    case D3DFMT_G16R16F:
    case D3DFMT_G16R16F_2:
        return RenderFormat::R16G16_FLOAT;
    case D3DFMT_INDEX16:
        return RenderFormat::R16_UINT;
    case D3DFMT_INDEX32:
        return RenderFormat::R32_UINT;
    case D3DFMT_L8:
    case D3DFMT_L8_2:
        return RenderFormat::R8_UNORM;
    default:
        assert(false && "Unknown format");
        return RenderFormat::R16G16B16A16_FLOAT;
    }
}

static GuestTexture* CreateTexture(uint32_t width, uint32_t height, uint32_t depth, uint32_t levels, uint32_t usage, uint32_t format, uint32_t pool, uint32_t type) 
{
    const auto texture = g_userHeap.AllocPhysical<GuestTexture>(type == 17 ? ResourceType::VolumeTexture : ResourceType::Texture);

    RenderTextureDesc desc;
    desc.dimension = texture->type == ResourceType::VolumeTexture ? RenderTextureDimension::TEXTURE_3D : RenderTextureDimension::TEXTURE_2D;
    desc.width = width;
    desc.height = height;
    desc.depth = depth;
    desc.mipLevels = levels;
    desc.arraySize = 1;
    desc.format = ConvertFormat(format);
    desc.flags = (desc.format == RenderFormat::D32_FLOAT) ? RenderTextureFlag::DEPTH_TARGET : RenderTextureFlag::NONE;

    texture->textureHolder = g_device->createTexture(desc);
    texture->texture = texture->textureHolder.get();

    RenderTextureViewDesc viewDesc;
    viewDesc.format = desc.format;
    viewDesc.dimension = texture->type == ResourceType::VolumeTexture ? RenderTextureViewDimension::TEXTURE_3D : RenderTextureViewDimension::TEXTURE_2D;
    viewDesc.mipLevels = levels;

    switch (format)
    {
    case D3DFMT_D24FS8:
    case D3DFMT_D24S8:
    case D3DFMT_L8:
    case D3DFMT_L8_2:
        viewDesc.componentMapping = RenderComponentMapping(RenderSwizzle::R, RenderSwizzle::R, RenderSwizzle::R, RenderSwizzle::ONE);
        break;

    case D3DFMT_X8R8G8B8:
        viewDesc.componentMapping = RenderComponentMapping(RenderSwizzle::G, RenderSwizzle::B, RenderSwizzle::A, RenderSwizzle::ONE);
        break;
    }

    texture->textureView = texture->texture->createTextureView(viewDesc);

    texture->width = width;
    texture->height = height;
    texture->depth = depth;
    texture->format = desc.format;
    texture->descriptorIndex = g_textureDescriptorAllocator.allocate();

    g_textureDescriptorSet->setTexture(texture->descriptorIndex, texture->texture, RenderTextureLayout::SHADER_READ, texture->textureView.get());
   
#ifdef _DEBUG 
    texture->texture->setName(std::format("Texture {:X}", g_memory.MapVirtual(texture)));
#endif

    return texture;
}

static GuestBuffer* CreateVertexBuffer(uint32_t length) 
{
    auto buffer = g_userHeap.AllocPhysical<GuestBuffer>(ResourceType::VertexBuffer);
    buffer->buffer = g_device->createBuffer(RenderBufferDesc::VertexBuffer(length, RenderHeapType::DEFAULT, RenderBufferFlag::INDEX));
    buffer->dataSize = length;
#ifdef _DEBUG 
    buffer->buffer->setName(std::format("Vertex Buffer {:X}", g_memory.MapVirtual(buffer)));
#endif
    return buffer;
}

static GuestBuffer* CreateIndexBuffer(uint32_t length, uint32_t, uint32_t format)
{
    auto buffer = g_userHeap.AllocPhysical<GuestBuffer>(ResourceType::IndexBuffer);
    buffer->buffer = g_device->createBuffer(RenderBufferDesc::IndexBuffer(length, RenderHeapType::DEFAULT));
    buffer->dataSize = length;
    buffer->format = ConvertFormat(format);
    buffer->guestFormat = format;
#ifdef _DEBUG 
    buffer->buffer->setName(std::format("Index Buffer {:X}", g_memory.MapVirtual(buffer)));
#endif
    return buffer;
}

static GuestSurface* CreateSurface(uint32_t width, uint32_t height, uint32_t format, uint32_t multiSample) 
{
    RenderTextureDesc desc;
    desc.dimension = RenderTextureDimension::TEXTURE_2D;
    desc.width = width;
    desc.height = height;
    desc.depth = 1;
    desc.mipLevels = 1;
    desc.arraySize = 1;
    desc.multisampling.sampleCount = multiSample != 0 && Config::MSAA > 1 ? Config::MSAA : RenderSampleCount::COUNT_1;
    desc.format = ConvertFormat(format);
    desc.flags = desc.format == RenderFormat::D32_FLOAT ? RenderTextureFlag::DEPTH_TARGET : RenderTextureFlag::RENDER_TARGET;

    auto surface = g_userHeap.AllocPhysical<GuestSurface>(desc.format == RenderFormat::D32_FLOAT ? 
        ResourceType::DepthStencil : ResourceType::RenderTarget);

    surface->textureHolder = g_device->createTexture(desc);
    surface->texture = surface->textureHolder.get();
    surface->width = width;
    surface->height = height;
    surface->format = desc.format;
    surface->sampleCount = desc.multisampling.sampleCount;

    if (desc.multisampling.sampleCount != RenderSampleCount::COUNT_1 && desc.format == RenderFormat::D32_FLOAT)
    {
        RenderTextureViewDesc viewDesc;
        viewDesc.dimension = RenderTextureViewDimension::TEXTURE_2D;
        viewDesc.format = RenderFormat::D32_FLOAT;
        viewDesc.mipLevels = 1;
        surface->textureView = surface->textureHolder->createTextureView(viewDesc);
        surface->descriptorIndex = g_textureDescriptorAllocator.allocate();
        g_textureDescriptorSet->setTexture(surface->descriptorIndex, surface->textureHolder.get(), RenderTextureLayout::SHADER_READ, surface->textureView.get());
    }

#ifdef _DEBUG 
    surface->texture->setName(std::format("{} {:X}", desc.flags & RenderTextureFlag::RENDER_TARGET ? "Render Target" : "Depth Stencil", g_memory.MapVirtual(surface)));
#endif

    return surface;
}

static void FlushViewport()
{
    bool renderingToBackBuffer = g_renderTarget == g_backBuffer &&
        g_backBuffer->texture != g_backBuffer->textureHolder.get();

    auto& commandList = g_commandLists[g_frame];

    if (g_dirtyStates.viewport)
    {
        auto viewport = g_viewport;
        if (g_halfPixel)
        {
            viewport.x += 0.5f;
            viewport.y += 0.5f;
        }

        if (renderingToBackBuffer)
        {
            uint32_t width = g_swapChain->getWidth();
            uint32_t height = g_swapChain->getHeight();

            viewport.x *= width / 1280.0f;
            viewport.y *= height / 720.0f;    
            viewport.width *= width / 1280.0f;
            viewport.height *= height / 720.0f;
        }

        commandList->setViewports(viewport);

        g_dirtyStates.viewport = false;
    }

    if (g_dirtyStates.scissorRect)
    {
        auto scissorRect = g_scissorTestEnable ? g_scissorRect : RenderRect(
            g_viewport.x,
            g_viewport.y,
            g_viewport.x + g_viewport.width,
            g_viewport.y + g_viewport.height);

        if (renderingToBackBuffer)
        {
            uint32_t width = g_swapChain->getWidth();
            uint32_t height = g_swapChain->getHeight();

            scissorRect.left = scissorRect.left * width / 1280;
            scissorRect.top = scissorRect.top * height / 720;
            scissorRect.right = scissorRect.right * width / 1280;
            scissorRect.bottom = scissorRect.bottom * height / 720;
        }

        commandList->setScissors(scissorRect);

        g_dirtyStates.scissorRect = false;
    }
}

static bool SetHalfPixel(bool enable)
{
    bool oldValue = g_halfPixel;
    SetDirtyValue(g_dirtyStates.viewport, g_halfPixel, enable);
    return oldValue;
}

static void StretchRect(GuestDevice* device, uint32_t flags, uint32_t, GuestTexture* texture)
{
    const bool isDepthStencil = (flags & 0x4) != 0;
    const auto surface = isDepthStencil ? g_depthStencil : g_renderTarget;
    const bool multiSampling = surface->sampleCount != RenderSampleCount::COUNT_1;

    RenderTextureLayout srcLayout;
    RenderTextureLayout dstLayout;

    if (multiSampling)
    {
        if (isDepthStencil)
        {
            srcLayout = RenderTextureLayout::SHADER_READ;
            dstLayout = RenderTextureLayout::DEPTH_WRITE;
        }
        else
        {
            srcLayout = RenderTextureLayout::RESOLVE_SOURCE;
            dstLayout = RenderTextureLayout::RESOLVE_DEST;
        }
    }
    else
    {
        srcLayout = RenderTextureLayout::COPY_SOURCE;
        dstLayout = RenderTextureLayout::COPY_DEST;
    }

    AddBarrier(surface, srcLayout);
    AddBarrier(texture, dstLayout);
    FlushBarriers();

    auto& commandList = g_commandLists[g_frame];
    if (multiSampling)
    {
        if (isDepthStencil)
        {
            uint32_t pipelineIndex = 0;

            switch (g_depthStencil->sampleCount)
            {
            case RenderSampleCount::COUNT_2:
                pipelineIndex = 0;
                break;    
            case RenderSampleCount::COUNT_4:
                pipelineIndex = 1;
                break;          
            case RenderSampleCount::COUNT_8:
                pipelineIndex = 2;
                break;
            default:
                assert(false && "Unsupported MSAA sample count");
                break;
            }

            if (texture->framebuffer == nullptr)
            {
                RenderFramebufferDesc desc;
                desc.depthAttachment = texture->texture;
                texture->framebuffer = g_device->createFramebuffer(desc);
            }
            
            bool oldHalfPixel = SetHalfPixel(false);
            FlushViewport();

            commandList->setFramebuffer(texture->framebuffer.get());
            commandList->setPipeline(g_resolveMsaaDepthPipelines[pipelineIndex].get());
            commandList->setGraphicsPushConstants(0, &g_depthStencil->descriptorIndex, 0, sizeof(uint32_t));
            commandList->drawInstanced(6, 1, 0, 0);

            g_dirtyStates.renderTargetAndDepthStencil = true;
            g_dirtyStates.pipelineState = true;

            if (g_vulkan)
                g_dirtyStates.vertexShaderConstants = true;

            SetHalfPixel(oldHalfPixel);
        }
        else
        {
            commandList->resolveTexture(texture->texture, surface->texture);
        }
    }
    else
    {
        commandList->copyTexture(texture->texture, surface->texture);
    }

    AddBarrier(texture, RenderTextureLayout::SHADER_READ);
}

static void SetRenderTarget(GuestDevice* device, uint32_t index, GuestSurface* renderTarget) 
{
    SetDirtyValue(g_dirtyStates.renderTargetAndDepthStencil, g_renderTarget, renderTarget);
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.renderTargetFormat, renderTarget != nullptr ? renderTarget->format : RenderFormat::UNKNOWN);
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.sampleCount, renderTarget != nullptr ? renderTarget->sampleCount : RenderSampleCount::COUNT_1);

    // When alpha to coverage is enabled, update the alpha test mode as it's dependent on sample count.
    SetAlphaTestMode(g_sharedConstants.alphaTestMode != AlphaTestMode::Disabled);
}

static GuestSurface* GetDepthStencilSurface(GuestDevice* device) 
{
    return nullptr;
}

static void SetDepthStencilSurface(GuestDevice* device, GuestSurface* depthStencil) 
{
    SetDirtyValue(g_dirtyStates.renderTargetAndDepthStencil, g_depthStencil, depthStencil);
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.depthStencilFormat, depthStencil != nullptr ? depthStencil->format : RenderFormat::UNKNOWN);
}

static void FlushFramebuffer()
{
    auto& commandList = g_commandLists[g_frame];

    AddBarrier(g_renderTarget, RenderTextureLayout::COLOR_WRITE);
    AddBarrier(g_depthStencil, RenderTextureLayout::DEPTH_WRITE);
    FlushBarriers();

    if (g_dirtyStates.renderTargetAndDepthStencil)
    {
        GuestSurface* framebufferContainer = nullptr;
        RenderTexture* framebufferKey = nullptr;

        if (g_renderTarget != nullptr && g_depthStencil != nullptr)
        {
            framebufferContainer = g_depthStencil; // Backbuffer texture changes per frame so we can't use the depth stencil as the key.
            framebufferKey = g_renderTarget->texture;
        }
        else if (g_renderTarget != nullptr && g_depthStencil == nullptr)
        {
            framebufferContainer = g_renderTarget;
            framebufferKey = g_renderTarget->texture; // Backbuffer texture changes per frame so we can't assume nullptr for it.
        }
        else if (g_renderTarget == nullptr && g_depthStencil != nullptr)
        {
            framebufferContainer = g_depthStencil;
            framebufferKey = nullptr;
        }

        if (framebufferContainer != nullptr)
        {
            auto& framebuffer = framebufferContainer->framebuffers[framebufferKey];

            if (framebuffer == nullptr)
            {
                RenderFramebufferDesc desc;

                if (g_renderTarget != nullptr)
                {
                    desc.colorAttachments = const_cast<const RenderTexture**>(&g_renderTarget->texture);
                    desc.colorAttachmentsCount = 1;
                }

                if (g_depthStencil != nullptr)
                    desc.depthAttachment = g_depthStencil->texture;

                framebuffer = g_device->createFramebuffer(desc);
            }

            commandList->setFramebuffer(framebuffer.get());
        }
        else
        {
            commandList->setFramebuffer(nullptr);
        }

        g_dirtyStates.renderTargetAndDepthStencil = false;
    }
}

static void Clear(GuestDevice* device, uint32_t flags, uint32_t, be<float>* color, double z) 
{
    FlushFramebuffer();

    auto& commandList = g_commandLists[g_frame];

    if (g_renderTarget != nullptr && (flags & D3DCLEAR_TARGET) != 0)
        commandList->clearColor(0, RenderColor(color[0], color[1], color[2], color[3]));

    if (g_depthStencil != nullptr && (flags & D3DCLEAR_ZBUFFER) != 0)
        commandList->clearDepth(true, float(z));
}

static void SetViewport(GuestDevice* device, GuestViewport* viewport)
{
    SetDirtyValue<float>(g_dirtyStates.viewport, g_viewport.x, viewport->x);
    SetDirtyValue<float>(g_dirtyStates.viewport, g_viewport.y, viewport->y);
    SetDirtyValue<float>(g_dirtyStates.viewport, g_viewport.width, viewport->width);
    SetDirtyValue<float>(g_dirtyStates.viewport, g_viewport.height, viewport->height);
    SetDirtyValue<float>(g_dirtyStates.viewport, g_viewport.minDepth, viewport->minZ);
    SetDirtyValue<float>(g_dirtyStates.viewport, g_viewport.maxDepth, viewport->maxZ);

    g_dirtyStates.scissorRect |= g_dirtyStates.viewport;
}

static void GetViewport(GuestDevice* device, GuestViewport* viewport)
{
    viewport->x = g_viewport.x;
    viewport->y = g_viewport.y;
    viewport->width = g_viewport.width;
    viewport->height = g_viewport.height;
    viewport->minZ = g_viewport.minDepth;
    viewport->maxZ = g_viewport.maxDepth;
}

static void SetTexture(GuestDevice* device, uint32_t index, GuestTexture* texture) 
{
    AddBarrier(texture, RenderTextureLayout::SHADER_READ);
    SetDirtyValue(g_dirtyStates.sharedConstants, g_sharedConstants.textureIndices[index], texture != nullptr ? texture->descriptorIndex : NULL);
}

static void SetScissorRect(GuestDevice* device, GuestRect* rect)
{
    SetDirtyValue<int32_t>(g_dirtyStates.scissorRect, g_scissorRect.top, rect->top);
    SetDirtyValue<int32_t>(g_dirtyStates.scissorRect, g_scissorRect.left, rect->left);
    SetDirtyValue<int32_t>(g_dirtyStates.scissorRect, g_scissorRect.bottom, rect->bottom);
    SetDirtyValue<int32_t>(g_dirtyStates.scissorRect, g_scissorRect.right, rect->right);
}

static RenderPipeline* CreateGraphicsPipeline(const PipelineState& pipelineState)
{
    auto& pipeline = g_pipelines[XXH3_64bits(&pipelineState, sizeof(PipelineState))];
    if (pipeline == nullptr)
    {
        RenderGraphicsPipelineDesc desc;
        desc.pipelineLayout = g_pipelineLayout.get();
        desc.vertexShader = pipelineState.vertexShader->shader.get();
        desc.pixelShader = pipelineState.pixelShader != nullptr ? pipelineState.pixelShader->shader.get() : nullptr;
        desc.depthFunction = pipelineState.zFunc;
        desc.depthEnabled = pipelineState.zEnable;
        desc.depthWriteEnabled = pipelineState.zWriteEnable;
        desc.depthBias = pipelineState.depthBias;
        desc.slopeScaledDepthBias = pipelineState.slopeScaledDepthBias;
        desc.depthClipEnabled = true;
        desc.primitiveTopology = pipelineState.primitiveTopology;
        desc.cullMode = pipelineState.cullMode;
        desc.renderTargetFormat[0] = pipelineState.renderTargetFormat;
        desc.renderTargetBlend[0].blendEnabled = pipelineState.alphaBlendEnable;
        desc.renderTargetBlend[0].srcBlend = pipelineState.srcBlend;
        desc.renderTargetBlend[0].dstBlend = pipelineState.destBlend;
        desc.renderTargetBlend[0].blendOp = pipelineState.blendOp;
        desc.renderTargetBlend[0].srcBlendAlpha = pipelineState.srcBlendAlpha;
        desc.renderTargetBlend[0].dstBlendAlpha = pipelineState.destBlendAlpha;
        desc.renderTargetBlend[0].blendOpAlpha = pipelineState.blendOpAlpha;
        desc.renderTargetBlend[0].renderTargetWriteMask = pipelineState.colorWriteEnable;
        desc.renderTargetCount = pipelineState.renderTargetFormat != RenderFormat::UNKNOWN ? 1 : 0;
        desc.depthTargetFormat = pipelineState.depthStencilFormat;
        desc.multisampling.sampleCount = pipelineState.sampleCount;
        desc.alphaToCoverageEnabled = pipelineState.enableAlphaToCoverage;
        desc.inputElements = pipelineState.vertexDeclaration->inputElements.get();
        desc.inputElementsCount = pipelineState.vertexDeclaration->inputElementCount;

        RenderInputSlot inputSlots[16]{};
        uint32_t inputSlotIndices[16]{};
        uint32_t inputSlotCount = 0;

        for (size_t i = 0; i < pipelineState.vertexDeclaration->inputElementCount; i++)
        {
            auto& inputElement = pipelineState.vertexDeclaration->inputElements[i];
            auto& inputSlotIndex = inputSlotIndices[inputElement.slotIndex];

            if (inputSlotIndex == NULL)
                inputSlotIndex = ++inputSlotCount;

            auto& inputSlot = inputSlots[inputSlotIndex - 1];
            inputSlot.index = inputElement.slotIndex;
            inputSlot.stride = pipelineState.vertexStrides[inputElement.slotIndex];

            if (pipelineState.instancing && inputElement.slotIndex != 0 && inputElement.slotIndex != 15)
                inputSlot.classification = RenderInputSlotClassification::PER_INSTANCE_DATA;
            else
                inputSlot.classification = RenderInputSlotClassification::PER_VERTEX_DATA;
        }

        desc.inputSlots = inputSlots;
        desc.inputSlotsCount = inputSlotCount;

        pipeline = g_device->createGraphicsPipeline(desc);
    }
    return pipeline.get();
}

static RenderTextureAddressMode ConvertTextureAddressMode(size_t value)
{
    switch (value)
    {
    case D3DTADDRESS_WRAP:
        return RenderTextureAddressMode::WRAP;
    case D3DTADDRESS_MIRROR:
        return RenderTextureAddressMode::MIRROR;
    case D3DTADDRESS_CLAMP:
        return RenderTextureAddressMode::CLAMP;
    case D3DTADDRESS_MIRRORONCE:
        return RenderTextureAddressMode::MIRROR_ONCE;
    case D3DTADDRESS_BORDER:
        return RenderTextureAddressMode::BORDER;
    default:
        assert(false && "Unknown texture address mode");
        return RenderTextureAddressMode::UNKNOWN;
    }
}

static RenderFilter ConvertTextureFilter(uint32_t value)
{
    switch (value)
    {
    case D3DTEXF_POINT:
    case D3DTEXF_NONE:
        return RenderFilter::NEAREST;
    case D3DTEXF_LINEAR:
        return RenderFilter::LINEAR;
    default:
        assert(false && "Unknown texture filter");
        return RenderFilter::UNKNOWN;
    }
}

static RenderBorderColor ConvertBorderColor(uint32_t value)
{
    switch (value)
    {
    case 0:
        return RenderBorderColor::TRANSPARENT_BLACK;
    case 1:
        return RenderBorderColor::OPAQUE_WHITE;
    default:
        assert(false && "Unknown border color");
        return RenderBorderColor::UNKNOWN;
    }
}

static void FlushRenderState(GuestDevice* device)
{
    FlushFramebuffer();
    FlushViewport();

    auto& commandList = g_commandLists[g_frame];

    if (g_dirtyStates.pipelineState)
        commandList->setPipeline(CreateGraphicsPipeline(g_pipelineState));

    constexpr size_t BOOL_MASK = 0x100000000000000ull;
    if ((device->dirtyFlags[4].get() & BOOL_MASK) != 0)
    {
        uint32_t booleans = device->vertexShaderBoolConstants[0].get() & 0xFF;
        booleans |= (device->pixelShaderBoolConstants[0].get() & 0xFF) << 16;

        SetDirtyValue(g_dirtyStates.sharedConstants, g_sharedConstants.booleans, booleans);

        device->dirtyFlags[4] = device->dirtyFlags[4].get() & ~BOOL_MASK;
    }

    for (size_t i = 0; i < 16; i++)
    {
        const size_t mask = 0x8000000000000000ull >> (i + 32);
        if (device->dirtyFlags[3].get() & mask)
        {
            const auto addressU = ConvertTextureAddressMode((device->samplerStates[i].data[0].get() >> 10) & 0x7);
            const auto addressV = ConvertTextureAddressMode((device->samplerStates[i].data[0].get() >> 13) & 0x7);
            const auto addressW = ConvertTextureAddressMode((device->samplerStates[i].data[0].get() >> 16) & 0x7);
            auto magFilter = ConvertTextureFilter((device->samplerStates[i].data[3].get() >> 19) & 0x3);
            auto minFilter = ConvertTextureFilter((device->samplerStates[i].data[3].get() >> 21) & 0x3);
            auto mipFilter = ConvertTextureFilter((device->samplerStates[i].data[3].get() >> 23) & 0x3);
            const auto borderColor = ConvertBorderColor(device->samplerStates[i].data[5].get() & 0x3);

            bool anisotropyEnabled = mipFilter == RenderFilter::LINEAR;
            if (anisotropyEnabled)
            {
                magFilter = RenderFilter::LINEAR;
                minFilter = RenderFilter::LINEAR;
            }

            auto& samplerDesc = g_samplerDescs[i];

            bool dirty = false;

            SetDirtyValue(dirty, samplerDesc.addressU, addressU);
            SetDirtyValue(dirty, samplerDesc.addressV, addressV);
            SetDirtyValue(dirty, samplerDesc.addressW, addressW);
            SetDirtyValue(dirty, samplerDesc.minFilter, minFilter);
            SetDirtyValue(dirty, samplerDesc.magFilter, magFilter);
            SetDirtyValue(dirty, samplerDesc.mipmapMode, RenderMipmapMode(mipFilter));
            SetDirtyValue(dirty, samplerDesc.anisotropyEnabled, anisotropyEnabled);
            SetDirtyValue(dirty, samplerDesc.borderColor, borderColor);

            if (dirty)
            {
                auto& [descriptorIndex, sampler] = g_samplerStates[XXH3_64bits(&samplerDesc, sizeof(RenderSamplerDesc))];
                if (descriptorIndex == NULL)
                {
                    descriptorIndex = g_samplerStates.size();
                    sampler = g_device->createSampler(samplerDesc);

                    g_samplerDescriptorSet->setSampler(descriptorIndex - 1, sampler.get());
                }

                SetDirtyValue(g_dirtyStates.sharedConstants, g_sharedConstants.samplerIndices[i], descriptorIndex - 1);
            }

            device->dirtyFlags[3] = device->dirtyFlags[3].get() & ~mask;
        }
    }

    auto& uploadAllocator = g_uploadAllocators[g_frame];

    auto setRootDescriptor = [&](const UploadAllocation& allocation, size_t index)
        {
            if (g_vulkan)
                commandList->setGraphicsPushConstants(0, &allocation.deviceAddress, 8 * index, 8);
            else
                commandList->setGraphicsRootDescriptor(allocation.bufferReference, index);
        };

    if (g_dirtyStates.sharedConstants)
    {
        auto sharedConstants = uploadAllocator.allocate<false>(&g_sharedConstants, sizeof(g_sharedConstants), 0x100);
        setRootDescriptor(sharedConstants, 2);
    }

    if (g_dirtyStates.vertexShaderConstants || device->dirtyFlags[0] != 0)
    {
        auto vertexShaderConstants = uploadAllocator.allocate<true>(device->vertexShaderFloatConstants, 0x1000, 0x100);
        setRootDescriptor(vertexShaderConstants, 0);
        device->dirtyFlags[0] = 0;
    }

    if (g_dirtyStates.vertexStreamFirst <= g_dirtyStates.vertexStreamLast)
    {
        commandList->setVertexBuffers(
            g_dirtyStates.vertexStreamFirst,
            g_vertexBufferViews + g_dirtyStates.vertexStreamFirst,
            g_dirtyStates.vertexStreamLast - g_dirtyStates.vertexStreamFirst + 1,
            g_inputSlots + g_dirtyStates.vertexStreamFirst);
    }

    if (g_dirtyStates.indices && (!g_vulkan || g_indexBufferView.buffer.ref != nullptr))
        commandList->setIndexBuffer(&g_indexBufferView);

    if (g_dirtyStates.pixelShaderConstants || device->dirtyFlags[1] != 0)
    {
        auto pixelShaderConstants = uploadAllocator.allocate<true>(device->pixelShaderFloatConstants, 0xE00, 0x100);
        setRootDescriptor(pixelShaderConstants, 1);
        device->dirtyFlags[1] = 0;
    }

    g_dirtyStates = DirtyStates(false);
}

static RenderPrimitiveTopology ConvertPrimitiveType(uint32_t primitiveType)
{
    switch (primitiveType)
    {
    case D3DPT_POINTLIST:
        return RenderPrimitiveTopology::POINT_LIST;   
    case D3DPT_LINELIST:
        return RenderPrimitiveTopology::LINE_LIST;   
    case D3DPT_LINESTRIP:
        return RenderPrimitiveTopology::LINE_STRIP;   
    case D3DPT_TRIANGLELIST:
    case D3DPT_QUADLIST:
        return RenderPrimitiveTopology::TRIANGLE_LIST;  
    case D3DPT_TRIANGLESTRIP:
        return RenderPrimitiveTopology::TRIANGLE_STRIP;   
    case D3DPT_TRIANGLEFAN:
        return RenderPrimitiveTopology::TRIANGLE_FAN;
    default:
        assert(false && "Unknown primitive type");
        return RenderPrimitiveTopology::UNKNOWN;
    }
}

static void SetPrimitiveType(uint32_t primitiveType)
{
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.primitiveTopology, ConvertPrimitiveType(primitiveType));
}

static uint32_t CheckInstancing()
{
    uint32_t indexCount = 0;

    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.instancing, g_pipelineState.vertexDeclaration->indexVertexStream != 0);
    if (g_pipelineState.instancing)
    {
        // Index buffer is passed as a vertex stream
        indexCount = g_vertexBufferViews[g_pipelineState.vertexDeclaration->indexVertexStream].size / 4;
    }

    return indexCount;
}

static void DrawPrimitive(GuestDevice* device, uint32_t primitiveType, uint32_t startVertex, uint32_t primitiveCount) 
{
    SetPrimitiveType(primitiveType);

    uint32_t indexCount = CheckInstancing();
    if (indexCount > 0)
    {
        auto& vertexBufferView = g_vertexBufferViews[g_pipelineState.vertexDeclaration->indexVertexStream];

        SetDirtyValue(g_dirtyStates.indices, g_indexBufferView.buffer, vertexBufferView.buffer);
        SetDirtyValue(g_dirtyStates.indices, g_indexBufferView.size, vertexBufferView.size);
        SetDirtyValue(g_dirtyStates.indices, g_indexBufferView.format, RenderFormat::R32_UINT);
    }

    FlushRenderState(device);

    auto& commandList = g_commandLists[g_frame];

    if (indexCount > 0)
        commandList->drawIndexedInstanced(indexCount, primitiveCount / indexCount, 0, 0, 0);
    else
        commandList->drawInstanced(primitiveCount, 1, startVertex, 0);
}

static void DrawIndexedPrimitive(GuestDevice* device, uint32_t primitiveType, int32_t baseVertexIndex, uint32_t startIndex, uint32_t primCount)
{
    CheckInstancing();
    SetPrimitiveType(primitiveType);
    FlushRenderState(device);
    g_commandLists[g_frame]->drawIndexedInstanced(primCount, 1, startIndex, baseVertexIndex, 0);
}

static void DrawPrimitiveUP(GuestDevice* device, uint32_t primitiveType, uint32_t primitiveCount, void* vertexStreamZeroData, uint32_t vertexStreamZeroStride)
{
    CheckInstancing();
    SetPrimitiveType(primitiveType);
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.vertexStrides[0], uint8_t(vertexStreamZeroStride));

    auto& vertexBufferView = g_vertexBufferViews[0];
    vertexBufferView.size = primitiveCount * vertexStreamZeroStride;
    vertexBufferView.buffer = g_uploadAllocators[g_frame].allocate<true>(reinterpret_cast<uint32_t*>(vertexStreamZeroData), vertexBufferView.size, 0x4).bufferReference;
    g_inputSlots[0].stride = vertexStreamZeroStride;
    g_dirtyStates.vertexStreamFirst = 0;

    if (primitiveType == D3DPT_QUADLIST)
    {
        static std::vector<uint16_t> quadIndexData;
        const uint32_t quadCount = primitiveCount / 4;
        const uint32_t triangleCount = quadCount * 6;

        if (quadIndexData.size() < triangleCount)
        {
            const size_t oldQuadCount = quadIndexData.size() / 6;
            quadIndexData.resize(triangleCount);

            for (size_t i = oldQuadCount; i < quadCount; i++)
            {
                quadIndexData[i * 6 + 0] = static_cast<uint16_t>(i * 4 + 0);
                quadIndexData[i * 6 + 1] = static_cast<uint16_t>(i * 4 + 1);
                quadIndexData[i * 6 + 2] = static_cast<uint16_t>(i * 4 + 2);

                quadIndexData[i * 6 + 3] = static_cast<uint16_t>(i * 4 + 0);
                quadIndexData[i * 6 + 4] = static_cast<uint16_t>(i * 4 + 2);
                quadIndexData[i * 6 + 5] = static_cast<uint16_t>(i * 4 + 3);
            }
        }

        if (g_quadIndexBuffer == NULL || g_quadCount < quadCount)
        {
            g_quadIndexBuffer = g_uploadAllocators[g_frame].allocate<false>(quadIndexData.data(), triangleCount * 2, 2).bufferReference;
            g_quadCount = quadCount;
        }

        SetDirtyValue(g_dirtyStates.indices, g_indexBufferView.buffer, g_quadIndexBuffer);
        SetDirtyValue(g_dirtyStates.indices, g_indexBufferView.size, g_quadCount * 12);
        SetDirtyValue(g_dirtyStates.indices, g_indexBufferView.format, RenderFormat::R16_UINT);

        FlushRenderState(device);
        g_commandLists[g_frame]->drawIndexedInstanced(triangleCount, 1, 0, 0, 0);
    }
    else
    {
        FlushRenderState(device);
        g_commandLists[g_frame]->drawInstanced(primitiveCount, 1, 0, 0);
    }
}

static const char* ConvertDeclUsage(uint32_t usage)
{
    switch (usage)
    {
    case D3DDECLUSAGE_POSITION:
        return "POSITION";
    case D3DDECLUSAGE_BLENDWEIGHT:
        return "BLENDWEIGHT";
    case D3DDECLUSAGE_BLENDINDICES:
        return "BLENDINDICES";
    case D3DDECLUSAGE_NORMAL:
        return "NORMAL";
    case D3DDECLUSAGE_PSIZE:
        return "PSIZE";
    case D3DDECLUSAGE_TEXCOORD:
        return "TEXCOORD";
    case D3DDECLUSAGE_TANGENT:
        return "TANGENT";
    case D3DDECLUSAGE_BINORMAL:
        return "BINORMAL";
    case D3DDECLUSAGE_TESSFACTOR:
        return "TESSFACTOR";
    case D3DDECLUSAGE_POSITIONT:
        return "POSITIONT";
    case D3DDECLUSAGE_COLOR:
        return "COLOR";
    case D3DDECLUSAGE_FOG:
        return "FOG";
    case D3DDECLUSAGE_DEPTH:
        return "DEPTH";
    case D3DDECLUSAGE_SAMPLE:
        return "SAMPLE";
    default:
        assert(false && "Unknown usage");
        return "UNKNOWN";
    }
}

static RenderFormat ConvertDeclType(uint32_t type)
{
    switch (type)
    {
    case D3DDECLTYPE_FLOAT1:
        return RenderFormat::R32_FLOAT;
    case D3DDECLTYPE_FLOAT2:
        return RenderFormat::R32G32_FLOAT;
    case D3DDECLTYPE_FLOAT3:
        return RenderFormat::R32G32B32_FLOAT;
    case D3DDECLTYPE_FLOAT4:
        return RenderFormat::R32G32B32A32_FLOAT;
    case D3DDECLTYPE_D3DCOLOR:
        return RenderFormat::B8G8R8A8_UNORM;
    case D3DDECLTYPE_UBYTE4:
    case D3DDECLTYPE_UBYTE4_2:
        return RenderFormat::R8G8B8A8_UINT;
    case D3DDECLTYPE_SHORT2:
        return RenderFormat::R16G16_SINT;
    case D3DDECLTYPE_SHORT4:
        return RenderFormat::R16G16B16A16_SINT;
    case D3DDECLTYPE_UBYTE4N:
    case D3DDECLTYPE_UBYTE4N_2:
        return RenderFormat::R8G8B8A8_UNORM;
    case D3DDECLTYPE_SHORT2N:
        return RenderFormat::R16G16_SNORM;
    case D3DDECLTYPE_SHORT4N:
        return RenderFormat::R16G16B16A16_SNORM;
    case D3DDECLTYPE_USHORT2N:
        return RenderFormat::R16G16_UNORM;
    case D3DDECLTYPE_USHORT4N:
        return RenderFormat::R16G16B16A16_UNORM;
    case D3DDECLTYPE_UINT1:
        return RenderFormat::R32_UINT;
    case D3DDECLTYPE_DEC3N_2:
    case D3DDECLTYPE_DEC3N_3:
        return RenderFormat::R32_UINT;
    case D3DDECLTYPE_FLOAT16_2:
        return RenderFormat::R16G16_FLOAT;
    case D3DDECLTYPE_FLOAT16_4:
        return RenderFormat::R16G16B16A16_FLOAT;
    default:
        assert(false && "Unknown type");
        return RenderFormat::UNKNOWN;
    }
}

static GuestVertexDeclaration* CreateVertexDeclaration(GuestVertexElement* vertexElements) 
{
    size_t vertexElementCount = 0;
    auto vertexElement = vertexElements;

    while (vertexElement->stream != 0xFF && vertexElement->type != D3DDECLTYPE_UNUSED)
    {
        vertexElement->padding = 0;
        ++vertexElement;
        ++vertexElementCount;
    }

    std::lock_guard lock(g_vertexDeclarationMutex);

    auto& vertexDeclaration = g_vertexDeclarations[
        XXH3_64bits(vertexElements, vertexElementCount * sizeof(GuestVertexElement))];

    if (vertexDeclaration == nullptr)
    {
        vertexDeclaration = g_userHeap.AllocPhysical<GuestVertexDeclaration>(ResourceType::VertexDeclaration);

        static std::vector<RenderInputElement> inputElements;
        inputElements.clear();

        struct Location
        {
            uint32_t usage;
            uint32_t usageIndex;
            uint32_t location;
        };

        constexpr Location locations[] =
        {
            { D3DDECLUSAGE_POSITION, 0, 0 },
            { D3DDECLUSAGE_NORMAL, 0, 1 },
            { D3DDECLUSAGE_TANGENT, 0, 2 },
            { D3DDECLUSAGE_BINORMAL, 0, 3 },
            { D3DDECLUSAGE_TEXCOORD, 0, 4 },
            { D3DDECLUSAGE_TEXCOORD, 1, 5 },
            { D3DDECLUSAGE_TEXCOORD, 2, 6 },
            { D3DDECLUSAGE_TEXCOORD, 3, 7 },
            { D3DDECLUSAGE_COLOR, 0, 8 },
            { D3DDECLUSAGE_BLENDINDICES, 0, 9 },
            { D3DDECLUSAGE_BLENDWEIGHT, 0, 10 },
            { D3DDECLUSAGE_COLOR, 1, 11 },
            { D3DDECLUSAGE_TEXCOORD, 4, 12 },
            { D3DDECLUSAGE_TEXCOORD, 5, 13 },
            { D3DDECLUSAGE_TEXCOORD, 6, 14 },
            { D3DDECLUSAGE_TEXCOORD, 7, 15 },
            { D3DDECLUSAGE_POSITION, 1, 15 }
        };

        vertexElement = vertexElements;
        while (vertexElement->stream != 0xFF && vertexElement->type != D3DDECLTYPE_UNUSED)
        {
            if (vertexElement->usage == D3DDECLUSAGE_POSITION && vertexElement->usageIndex == 2)
            {
                ++vertexElement;
                continue;
            }

            auto& inputElement = inputElements.emplace_back();
            
            inputElement.semanticName = ConvertDeclUsage(vertexElement->usage);
            inputElement.semanticIndex = vertexElement->usageIndex;
            inputElement.location = ~0;

            for (auto& location : locations)
            {
                if (location.usage == vertexElement->usage && location.usageIndex == vertexElement->usageIndex)
                {
                    inputElement.location = location.location;
                    break;
                }
            }

            assert(inputElement.location != ~0);

            inputElement.format = ConvertDeclType(vertexElement->type);
            inputElement.slotIndex = vertexElement->stream;
            inputElement.alignedByteOffset = vertexElement->offset;

            switch (vertexElement->usage)
            {
            case D3DDECLUSAGE_POSITION:
                if (vertexElement->usageIndex == 1)
                    vertexDeclaration->indexVertexStream = vertexElement->stream;
                break;

            case D3DDECLUSAGE_BLENDWEIGHT:
            case D3DDECLUSAGE_BLENDINDICES:
                vertexDeclaration->inputLayoutFlags |= INPUT_LAYOUT_FLAG_HAS_BONE_WEIGHTS;
                break;

            case D3DDECLUSAGE_NORMAL:
            case D3DDECLUSAGE_TANGENT:
            case D3DDECLUSAGE_BINORMAL:
                if (vertexElement->type == D3DDECLTYPE_FLOAT3)
                    inputElement.format = RenderFormat::R32G32B32_UINT;
                else
                    vertexDeclaration->inputLayoutFlags |= INPUT_LAYOUT_FLAG_HAS_R11G11B10_NORMAL;
                break;

            case D3DDECLUSAGE_TEXCOORD:
                switch (vertexElement->type)
                {
                case D3DDECLTYPE_SHORT2:
                case D3DDECLTYPE_SHORT4:
                case D3DDECLTYPE_SHORT2N:
                case D3DDECLTYPE_SHORT4N:
                case D3DDECLTYPE_USHORT2N:
                case D3DDECLTYPE_USHORT4N:
                case D3DDECLTYPE_FLOAT16_2:
                case D3DDECLTYPE_FLOAT16_4:
                    vertexDeclaration->swappedTexcoords |= 1 << vertexElement->usageIndex;
                    break;
                }

                break;
            }

            ++vertexElement;
        }

        auto addInputElement = [&](uint32_t usage, uint32_t usageIndex)
            {
                uint32_t location = ~0;

                for (auto& alsoLocation : locations)
                {
                    if (alsoLocation.usage == usage && alsoLocation.usageIndex == usageIndex)
                    {
                        location = alsoLocation.location;
                        break;
                    }
                }

                assert(location != ~0);

                for (auto& inputElement : inputElements)
                {
                    if (inputElement.location == location)
                        return;
                }

                auto format = RenderFormat::R32_FLOAT;
                switch (usage)
                {
                case D3DDECLUSAGE_NORMAL:
                case D3DDECLUSAGE_TANGENT:
                case D3DDECLUSAGE_BINORMAL:
                case D3DDECLUSAGE_BLENDINDICES:
                    format = RenderFormat::R32_UINT;
                    break;
                }

                inputElements.emplace_back(ConvertDeclUsage(usage), usageIndex, location, format, 15, 0);
            };

        addInputElement(D3DDECLUSAGE_POSITION, 0);
        addInputElement(D3DDECLUSAGE_NORMAL, 0);
        addInputElement(D3DDECLUSAGE_TANGENT, 0);
        addInputElement(D3DDECLUSAGE_BINORMAL, 0);
        addInputElement(D3DDECLUSAGE_TEXCOORD, 0);
        addInputElement(D3DDECLUSAGE_TEXCOORD, 1);
        addInputElement(D3DDECLUSAGE_TEXCOORD, 2);
        addInputElement(D3DDECLUSAGE_TEXCOORD, 3);
        addInputElement(D3DDECLUSAGE_COLOR, 0);
        addInputElement(D3DDECLUSAGE_BLENDWEIGHT, 0);
        addInputElement(D3DDECLUSAGE_BLENDINDICES, 0);

        vertexDeclaration->inputElements = std::make_unique<RenderInputElement[]>(inputElements.size());
        std::copy(inputElements.begin(), inputElements.end(), vertexDeclaration->inputElements.get());

        vertexDeclaration->vertexElements = std::make_unique<GuestVertexElement[]>(vertexElementCount + 1);
        std::copy(vertexElements, vertexElements + vertexElementCount + 1, vertexDeclaration->vertexElements.get());

        vertexDeclaration->inputElementCount = uint32_t(inputElements.size());
        vertexDeclaration->vertexElementCount = vertexElementCount + 1;
    }

    vertexDeclaration->AddRef();
    return vertexDeclaration;
}

static void SetVertexDeclaration(GuestDevice* device, GuestVertexDeclaration* vertexDeclaration) 
{
    if (vertexDeclaration != nullptr)
    {
        SetDirtyValue(g_dirtyStates.sharedConstants, g_sharedConstants.swappedTexcoords, vertexDeclaration->swappedTexcoords);
        SetDirtyValue(g_dirtyStates.sharedConstants, g_sharedConstants.inputLayoutFlags, vertexDeclaration->inputLayoutFlags);
    }
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.vertexDeclaration, vertexDeclaration);
    device->vertexDeclaration = g_memory.MapVirtual(vertexDeclaration);
}

static GuestShader* CreateShader(const be<uint32_t>* function, ResourceType resourceType)
{
    XXH64_hash_t hash = XXH3_64bits(function, function[1] + function[2]);

    auto end = g_shaderCacheEntries + g_shaderCacheEntryCount;
    auto findResult = std::lower_bound(g_shaderCacheEntries, end, hash, [](ShaderCacheEntry& lhs, XXH64_hash_t rhs)
        {
            return lhs.hash < rhs;
        });

    GuestShader* shader = nullptr;

    if (findResult != end && findResult->hash == hash)
    {
        if (findResult->userData == nullptr)
        {
            shader = g_userHeap.AllocPhysical<GuestShader>(resourceType);

            if (g_vulkan)
                shader->shader = g_device->createShader(g_shaderCache.get() + findResult->spirvOffset, findResult->spirvSize, "main", RenderShaderFormat::SPIRV);
            else
                shader->shader = g_device->createShader(g_shaderCache.get() + findResult->dxilOffset, findResult->dxilSize, "main", RenderShaderFormat::DXIL);

            findResult->userData = shader;
        }
        else
        {
            shader = reinterpret_cast<GuestShader*>(findResult->userData);
        }
    }

    if (shader == nullptr)
        shader = g_userHeap.AllocPhysical<GuestShader>(resourceType);
    else
        shader->AddRef();

    return shader;
}

static GuestShader* CreateVertexShader(const be<uint32_t>* function) 
{
    return CreateShader(function, ResourceType::VertexShader);
}

static void SetVertexShader(GuestDevice* device, GuestShader* shader)
{
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.vertexShader, shader);
}

static void SetStreamSource(GuestDevice* device, uint32_t index, GuestBuffer* buffer, uint32_t offset, uint32_t stride) 
{
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.vertexStrides[index], uint8_t(buffer != nullptr ? stride : 0));

    bool dirty = false;

    SetDirtyValue(dirty, g_vertexBufferViews[index].buffer, buffer != nullptr ? buffer->buffer->at(offset) : RenderBufferReference{});
    SetDirtyValue(dirty, g_vertexBufferViews[index].size, buffer != nullptr ? (buffer->dataSize - offset) : 0u);
    SetDirtyValue(dirty, g_inputSlots[index].stride, buffer != nullptr ? stride : 0u);

    if (dirty)
    {
        g_dirtyStates.vertexStreamFirst = std::min<uint8_t>(g_dirtyStates.vertexStreamFirst, index);
        g_dirtyStates.vertexStreamLast = std::max<uint8_t>(g_dirtyStates.vertexStreamLast, index);
    }
}

static void SetIndices(GuestDevice* device, GuestBuffer* buffer) 
{
    SetDirtyValue(g_dirtyStates.indices, g_indexBufferView.buffer, buffer != nullptr ? buffer->buffer->at(0) : RenderBufferReference{});
    SetDirtyValue(g_dirtyStates.indices, g_indexBufferView.format, buffer != nullptr ? buffer->format : RenderFormat::R16_UINT);
    SetDirtyValue(g_dirtyStates.indices, g_indexBufferView.size, buffer != nullptr ? buffer->dataSize : 0u);
}

static GuestShader* CreatePixelShader(const be<uint32_t>* function)
{
    return CreateShader(function, ResourceType::PixelShader);
}

static void SetPixelShader(GuestDevice* device, GuestShader* shader)
{
    SetDirtyValue(g_dirtyStates.pipelineState, g_pipelineState.pixelShader, shader);
}

static void D3DXFillVolumeTexture(GuestTexture* texture, uint32_t function, void* data)
{
    uint32_t rowPitch0 = (texture->width * 4 + PITCH_ALIGNMENT - 1) & ~(PITCH_ALIGNMENT - 1);
    uint32_t slicePitch0 = (rowPitch0 * texture->height * texture->depth + PLACEMENT_ALIGNMENT - 1) & ~(PLACEMENT_ALIGNMENT - 1);

    uint32_t rowPitch1 = ((texture->width / 2) * 4 + PITCH_ALIGNMENT - 1) & ~(PITCH_ALIGNMENT - 1);
    uint32_t slicePitch1 = (rowPitch1 * (texture->height / 2) * (texture->depth / 2) + PLACEMENT_ALIGNMENT - 1) & ~(PLACEMENT_ALIGNMENT - 1);

    auto uploadBuffer = g_device->createBuffer(RenderBufferDesc::UploadBuffer(slicePitch0 + slicePitch1));
    uint8_t* mappedData = reinterpret_cast<uint8_t*>(uploadBuffer->map());

    thread_local std::vector<float> mipData;
    mipData.resize((texture->width / 2) * (texture->height / 2) * (texture->depth / 2) * 4);
    memset(mipData.data(), 0, mipData.size() * sizeof(float));

    for (size_t z = 0; z < texture->depth; z++)
    {
        for (size_t y = 0; y < texture->height; y++)
        {
            for (size_t x = 0; x < texture->width; x++)
            {
                auto dest = mappedData + z * rowPitch0 * texture->height + y * rowPitch0 + x * sizeof(uint32_t);
                size_t index = z * texture->width * texture->height + y * texture->width + x;
                size_t mipIndex = ((z / 2) * (texture->width / 2) * (texture->height / 2) + (y / 2) * (texture->width / 2) + x / 2) * 4;

                if (function == 0x82BC7820)
                {
                    auto src = reinterpret_cast<be<float>*>(data) + index * 4;

                    float r = static_cast<uint8_t>(src[0] * 255.0f);
                    float g = static_cast<uint8_t>(src[1] * 255.0f);
                    float b = static_cast<uint8_t>(src[2] * 255.0f);
                    float a = static_cast<uint8_t>(src[3] * 255.0f);

                    dest[0] = r;
                    dest[1] = g;
                    dest[2] = b;
                    dest[3] = a;

                    mipData[mipIndex + 0] += r;
                    mipData[mipIndex + 1] += g;
                    mipData[mipIndex + 2] += b;
                    mipData[mipIndex + 3] += a;
                }
                else if (function == 0x82BC78A8)
                {
                    auto src = reinterpret_cast<uint8_t*>(data) + index * 4;

                    dest[0] = src[3];
                    dest[1] = src[2];
                    dest[2] = src[1];
                    dest[3] = src[0];

                    mipData[mipIndex + 0] += src[3];
                    mipData[mipIndex + 1] += src[2];
                    mipData[mipIndex + 2] += src[1];
                    mipData[mipIndex + 3] += src[0];
                }
            }
        }
    }

    for (size_t z = 0; z < texture->depth / 2; z++)
    {
        for (size_t y = 0; y < texture->height / 2; y++)
        {
            for (size_t x = 0; x < texture->width / 2; x++)
            {
                auto dest = mappedData + slicePitch0 + z * rowPitch1 * (texture->height / 2) + y * rowPitch1 + x * sizeof(uint32_t);
                size_t index = (z * (texture->width / 2) * (texture->height / 2) + y * (texture->width / 2) + x) * 4;

                dest[0] = static_cast<uint8_t>(mipData[index + 0] / 8.0f);
                dest[1] = static_cast<uint8_t>(mipData[index + 1] / 8.0f);
                dest[2] = static_cast<uint8_t>(mipData[index + 2] / 8.0f);
                dest[3] = static_cast<uint8_t>(mipData[index + 3] / 8.0f);
            }
        }
    }

    ExecuteCopyCommandList([&]
        {
            g_copyCommandList->copyTextureRegion(
                RenderTextureCopyLocation::Subresource(texture->texture, 0),
                RenderTextureCopyLocation::PlacedFootprint(uploadBuffer.get(), texture->format, texture->width, texture->height, texture->depth, rowPitch0 / RenderFormatSize(texture->format), 0));

            g_copyCommandList->copyTextureRegion(
                RenderTextureCopyLocation::Subresource(texture->texture, 1),
                RenderTextureCopyLocation::PlacedFootprint(uploadBuffer.get(), texture->format, texture->width / 2, texture->height / 2, texture->depth / 2, rowPitch1 / RenderFormatSize(texture->format), slicePitch0));
        });
}

struct GuestPictureData
{
    be<uint32_t> vtable;
    uint8_t flags;
    be<uint32_t> name;
    be<uint32_t> texture;
    be<uint32_t> type;
};

static RenderTextureDimension ConvertTextureDimension(ddspp::TextureType type)
{
    switch (type) 
    {
    case ddspp::Texture1D:
        return RenderTextureDimension::TEXTURE_1D;
    case ddspp::Texture2D:
    case ddspp::Cubemap:
        return RenderTextureDimension::TEXTURE_2D;
    case ddspp::Texture3D:
        return RenderTextureDimension::TEXTURE_3D;
    default:
        assert(false && "Unknown texture type from DDS.");
        return RenderTextureDimension::UNKNOWN;
    }
}

static RenderTextureViewDimension ConvertTextureViewDimension(ddspp::TextureType type)
{
    switch (type)
    {
    case ddspp::Texture1D:
        return RenderTextureViewDimension::TEXTURE_1D;
    case ddspp::Texture2D:
        return RenderTextureViewDimension::TEXTURE_2D;
    case ddspp::Texture3D:
        return RenderTextureViewDimension::TEXTURE_3D;
    case ddspp::Cubemap:
        return RenderTextureViewDimension::TEXTURE_CUBE;
    default:
        assert(false && "Unknown texture type from DDS.");
        return RenderTextureViewDimension::UNKNOWN;
    }
}

static RenderFormat ConvertDXGIFormat(ddspp::DXGIFormat format) 
{
    switch (format)
    {
    case ddspp::R32G32B32A32_TYPELESS:
        return RenderFormat::R32G32B32A32_TYPELESS;
    case ddspp::R32G32B32A32_FLOAT:
        return RenderFormat::R32G32B32A32_FLOAT;
    case ddspp::R32G32B32A32_UINT:
        return RenderFormat::R32G32B32A32_UINT;
    case ddspp::R32G32B32A32_SINT:
        return RenderFormat::R32G32B32A32_SINT;
    case ddspp::R32G32B32_TYPELESS:
        return RenderFormat::R32G32B32_TYPELESS;
    case ddspp::R32G32B32_FLOAT:
        return RenderFormat::R32G32B32_FLOAT;
    case ddspp::R32G32B32_UINT:
        return RenderFormat::R32G32B32_UINT;
    case ddspp::R32G32B32_SINT:
        return RenderFormat::R32G32B32_SINT;
    case ddspp::R16G16B16A16_TYPELESS:
        return RenderFormat::R16G16B16A16_TYPELESS;
    case ddspp::R16G16B16A16_FLOAT:
        return RenderFormat::R16G16B16A16_FLOAT;
    case ddspp::R16G16B16A16_UNORM:
        return RenderFormat::R16G16B16A16_UNORM;
    case ddspp::R16G16B16A16_UINT:
        return RenderFormat::R16G16B16A16_UINT;
    case ddspp::R16G16B16A16_SNORM:
        return RenderFormat::R16G16B16A16_SNORM;
    case ddspp::R16G16B16A16_SINT:
        return RenderFormat::R16G16B16A16_SINT;
    case ddspp::R32G32_TYPELESS:
        return RenderFormat::R32G32_TYPELESS;
    case ddspp::R32G32_FLOAT:
        return RenderFormat::R32G32_FLOAT;
    case ddspp::R32G32_UINT:
        return RenderFormat::R32G32_UINT;
    case ddspp::R32G32_SINT:
        return RenderFormat::R32G32_SINT;
    case ddspp::R8G8B8A8_TYPELESS:
        return RenderFormat::R8G8B8A8_TYPELESS;
    case ddspp::R8G8B8A8_UNORM:
        return RenderFormat::R8G8B8A8_UNORM;
    case ddspp::R8G8B8A8_UINT:
        return RenderFormat::R8G8B8A8_UINT;
    case ddspp::R8G8B8A8_SNORM:
        return RenderFormat::R8G8B8A8_SNORM;
    case ddspp::R8G8B8A8_SINT:
        return RenderFormat::R8G8B8A8_SINT;
    case ddspp::B8G8R8A8_UNORM:
        return RenderFormat::B8G8R8A8_UNORM;
    case ddspp::B8G8R8X8_UNORM:
        return RenderFormat::B8G8R8A8_UNORM;   
    case ddspp::R16G16_TYPELESS:
        return RenderFormat::R16G16_TYPELESS;
    case ddspp::R16G16_FLOAT:
        return RenderFormat::R16G16_FLOAT;
    case ddspp::R16G16_UNORM:
        return RenderFormat::R16G16_UNORM;
    case ddspp::R16G16_UINT:
        return RenderFormat::R16G16_UINT;
    case ddspp::R16G16_SNORM:
        return RenderFormat::R16G16_SNORM;
    case ddspp::R16G16_SINT:
        return RenderFormat::R16G16_SINT;
    case ddspp::R32_TYPELESS:
        return RenderFormat::R32_TYPELESS;
    case ddspp::D32_FLOAT:
        return RenderFormat::D32_FLOAT;
    case ddspp::R32_FLOAT:
        return RenderFormat::R32_FLOAT;
    case ddspp::R32_UINT:
        return RenderFormat::R32_UINT;
    case ddspp::R32_SINT:
        return RenderFormat::R32_SINT;
    case ddspp::R8G8_TYPELESS:
        return RenderFormat::R8G8_TYPELESS;
    case ddspp::R8G8_UNORM:
        return RenderFormat::R8G8_UNORM;
    case ddspp::R8G8_UINT:
        return RenderFormat::R8G8_UINT;
    case ddspp::R8G8_SNORM:
        return RenderFormat::R8G8_SNORM;
    case ddspp::R8G8_SINT:
        return RenderFormat::R8G8_SINT;
    case ddspp::R16_TYPELESS:
        return RenderFormat::R16_TYPELESS;
    case ddspp::R16_FLOAT:
        return RenderFormat::R16_FLOAT;
    case ddspp::D16_UNORM:
        return RenderFormat::D16_UNORM;
    case ddspp::R16_UNORM:
        return RenderFormat::R16_UNORM;
    case ddspp::R16_UINT:
        return RenderFormat::R16_UINT;
    case ddspp::R16_SNORM:
        return RenderFormat::R16_SNORM;
    case ddspp::R16_SINT:
        return RenderFormat::R16_SINT;
    case ddspp::R8_TYPELESS:
        return RenderFormat::R8_TYPELESS;
    case ddspp::R8_UNORM:
        return RenderFormat::R8_UNORM;
    case ddspp::R8_UINT:
        return RenderFormat::R8_UINT;
    case ddspp::R8_SNORM:
        return RenderFormat::R8_SNORM;
    case ddspp::R8_SINT:
        return RenderFormat::R8_SINT;
    case ddspp::BC1_TYPELESS:
        return RenderFormat::BC1_TYPELESS;
    case ddspp::BC1_UNORM:
        return RenderFormat::BC1_UNORM;
    case ddspp::BC1_UNORM_SRGB:
        return RenderFormat::BC1_UNORM_SRGB;
    case ddspp::BC2_TYPELESS:
        return RenderFormat::BC2_TYPELESS;
    case ddspp::BC2_UNORM:
        return RenderFormat::BC2_UNORM;
    case ddspp::BC2_UNORM_SRGB:
        return RenderFormat::BC2_UNORM_SRGB;
    case ddspp::BC3_TYPELESS:
        return RenderFormat::BC3_TYPELESS;
    case ddspp::BC3_UNORM:
        return RenderFormat::BC3_UNORM;
    case ddspp::BC3_UNORM_SRGB:
        return RenderFormat::BC3_UNORM_SRGB;
    case ddspp::BC4_TYPELESS:
        return RenderFormat::BC4_TYPELESS;
    case ddspp::BC4_UNORM:
        return RenderFormat::BC4_UNORM;
    case ddspp::BC4_SNORM:
        return RenderFormat::BC4_SNORM;
    case ddspp::BC5_TYPELESS:
        return RenderFormat::BC5_TYPELESS;
    case ddspp::BC5_UNORM:
        return RenderFormat::BC5_UNORM;
    case ddspp::BC5_SNORM:
        return RenderFormat::BC5_SNORM;
    case ddspp::BC6H_TYPELESS:
        return RenderFormat::BC6H_TYPELESS;
    case ddspp::BC6H_UF16:
        return RenderFormat::BC6H_UF16;
    case ddspp::BC6H_SF16:
        return RenderFormat::BC6H_SF16;
    case ddspp::BC7_TYPELESS:
        return RenderFormat::BC7_TYPELESS;
    case ddspp::BC7_UNORM:
        return RenderFormat::BC7_UNORM;
    case ddspp::BC7_UNORM_SRGB:
        return RenderFormat::BC7_UNORM_SRGB;
    default:
        assert(false && "Unsupported format from DDS.");
        return RenderFormat::UNKNOWN;
    }
}

static void MakePictureData(GuestPictureData* pictureData, uint8_t* data, uint32_t dataSize)
{
    if ((pictureData->flags & 0x1) == 0)
    {
        ddspp::Descriptor ddsDesc;
        if (ddspp::decode_header(data, ddsDesc) != ddspp::Error)
        {
            const auto texture = g_userHeap.AllocPhysical<GuestTexture>(ResourceType::Texture);

            RenderTextureDesc desc;
            desc.dimension = ConvertTextureDimension(ddsDesc.type);
            desc.width = ddsDesc.width;
            desc.height = ddsDesc.height;
            desc.depth = ddsDesc.depth;
            desc.mipLevels = ddsDesc.numMips;
            desc.arraySize = ddsDesc.type == ddspp::TextureType::Cubemap ? ddsDesc.arraySize * 6 : ddsDesc.arraySize;
            desc.format = ConvertDXGIFormat(ddsDesc.format);
            desc.flags = ddsDesc.type == ddspp::TextureType::Cubemap ? RenderTextureFlag::CUBE : RenderTextureFlag::NONE;

            texture->textureHolder = g_device->createTexture(desc);
            texture->texture = texture->textureHolder.get();
            texture->layout = RenderTextureLayout::COPY_DEST;

#ifdef _DEBUG
            texture->texture->setName(reinterpret_cast<char*>(g_memory.Translate(pictureData->name + 2)));
#endif

            RenderTextureViewDesc viewDesc;
            viewDesc.format = desc.format;
            viewDesc.dimension = ConvertTextureViewDimension(ddsDesc.type);
            viewDesc.mipLevels = ddsDesc.numMips;
            texture->textureView = texture->texture->createTextureView(viewDesc);
            texture->descriptorIndex = g_textureDescriptorAllocator.allocate();
            g_textureDescriptorSet->setTexture(texture->descriptorIndex, texture->texture, RenderTextureLayout::SHADER_READ, texture->textureView.get());

            struct Slice
            {
                uint32_t width;
                uint32_t height;
                uint32_t depth;
                uint32_t srcOffset;
                uint32_t dstOffset;
                uint32_t srcRowPitch;
                uint32_t dstRowPitch;
                uint32_t rowCount;
            };

            std::vector<Slice> slices;
            uint32_t curSrcOffset = 0;
            uint32_t curDstOffset = 0;

            for (uint32_t arraySlice = 0; arraySlice < desc.arraySize; arraySlice++)
            {
                for (uint32_t mipSlice = 0; mipSlice < ddsDesc.numMips; mipSlice++)
                {
                    auto& slice = slices.emplace_back();

                    slice.width = std::max(1u, ddsDesc.width >> mipSlice);
                    slice.height = std::max(1u, ddsDesc.height >> mipSlice);
                    slice.depth = std::max(1u, ddsDesc.depth >> mipSlice);
                    slice.srcOffset = curSrcOffset;
                    slice.dstOffset = curDstOffset;
                    uint32_t rowPitch = ((slice.width + ddsDesc.blockWidth - 1) / ddsDesc.blockWidth) * ddsDesc.bitsPerPixelOrBlock;
                    slice.srcRowPitch = (rowPitch + 7) / 8;
                    slice.dstRowPitch = (slice.srcRowPitch + PITCH_ALIGNMENT - 1) & ~(PITCH_ALIGNMENT - 1);
                    slice.rowCount = (slice.height + ddsDesc.blockHeight - 1) / ddsDesc.blockHeight;

                    curSrcOffset += slice.srcRowPitch * slice.rowCount * slice.depth;
                    curDstOffset += (slice.dstRowPitch * slice.rowCount * slice.depth + PLACEMENT_ALIGNMENT - 1) & ~(PLACEMENT_ALIGNMENT - 1);
                }
            }

            auto uploadBuffer = g_device->createBuffer(RenderBufferDesc::UploadBuffer(curDstOffset));
            uint8_t* mappedMemory = reinterpret_cast<uint8_t*>(uploadBuffer->map());

            for (auto& slice : slices)
            {
                uint8_t* srcData = data + ddsDesc.headerSize + slice.srcOffset;
                uint8_t* dstData = mappedMemory + slice.dstOffset;

                if (slice.srcRowPitch == slice.dstRowPitch)
                {
                    memcpy(dstData, srcData, slice.srcRowPitch * slice.rowCount * slice.depth);
                }
                else
                {
                    for (size_t i = 0; i < slice.rowCount * slice.depth; i++)
                    {
                        memcpy(dstData, srcData, slice.srcRowPitch);
                        srcData += slice.srcRowPitch;
                        dstData += slice.dstRowPitch;
                    }
                }
            }

            uploadBuffer->unmap();

            ExecuteCopyCommandList([&]
                {
                    g_copyCommandList->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(texture->texture, RenderTextureLayout::COPY_DEST));

                    for (size_t i = 0; i < slices.size(); i++)
                    {
                        auto& slice = slices[i];

                        g_copyCommandList->copyTextureRegion(
                            RenderTextureCopyLocation::Subresource(texture->texture, i),
                            RenderTextureCopyLocation::PlacedFootprint(uploadBuffer.get(), desc.format, slice.width, slice.height, slice.depth, (slice.dstRowPitch * 8) / ddsDesc.bitsPerPixelOrBlock * ddsDesc.blockWidth, slice.dstOffset));
                    }
                });

            pictureData->texture = g_memory.MapVirtual(texture);
            pictureData->type = 0;
        }
    }
}

void IndexBufferLengthMidAsmHook(PPCRegister& r3)
{
    r3.u64 *= 2;
}

void SetShadowResolutionMidAsmHook(PPCRegister& r11)
{
    auto res = (int32_t)Config::ShadowResolution.Value;

    if (res > 0)
        r11.u64 = res;
}

static void SetResolution(be<uint32_t>* device)
{
    uint32_t width = uint32_t(g_swapChain->getWidth() * Config::ResolutionScale);
    uint32_t height = uint32_t(g_swapChain->getHeight() * Config::ResolutionScale);
    device[46] = width == 0 ? 880 : width;
    device[47] = height == 0 ? 720 : height;
}

static uint32_t StubFunction()
{
    return 0;
}

static GuestShader* g_movieVertexShader;
static GuestShader* g_moviePixelShader;
static GuestVertexDeclaration* g_movieVertexDeclaration;

static void ScreenShaderInit(be<uint32_t>* a1, uint32_t a2, uint32_t a3, GuestVertexElement* vertexElements)
{
    if (g_moviePixelShader == nullptr)
    {
        g_moviePixelShader = g_userHeap.AllocPhysical<GuestShader>(ResourceType::PixelShader);
        g_moviePixelShader->shader = CREATE_SHADER(movie_ps);
    }

    if (g_movieVertexShader == nullptr)
    {
        g_movieVertexShader = g_userHeap.AllocPhysical<GuestShader>(ResourceType::VertexShader);
        g_movieVertexShader->shader = CREATE_SHADER(movie_vs);
    }

    if (g_movieVertexDeclaration == nullptr)
        g_movieVertexDeclaration = CreateVertexDeclaration(vertexElements);

    g_moviePixelShader->AddRef();
    g_movieVertexShader->AddRef();
    g_movieVertexDeclaration->AddRef();

    a1[2] = g_memory.MapVirtual(g_moviePixelShader);
    a1[3] = g_memory.MapVirtual(g_movieVertexShader);
    a1[4] = g_memory.MapVirtual(g_movieVertexDeclaration);
}

void MovieRendererMidAsmHook(PPCRegister& r3)
{
    auto device = reinterpret_cast<GuestDevice*>(g_memory.Translate(r3.u32));

    // Force linear filtering & clamp addressing
    for (size_t i = 0; i < 3; i++)
    {
        device->samplerStates[i].data[0] = (device->samplerStates[i].data[0].get() & ~0x7fc00) | 0x24800;
        device->samplerStates[i].data[3] = (device->samplerStates[i].data[3].get() & ~0x1f80000) | 0x1280000;
    }

    device->dirtyFlags[3] = device->dirtyFlags[3].get() | 0xe0000000ull;
}

GUEST_FUNCTION_HOOK(sub_82BD99B0, CreateDevice);

GUEST_FUNCTION_HOOK(sub_82BE6230, DestructResource);

GUEST_FUNCTION_HOOK(sub_82BE9300, LockTextureRect);
GUEST_FUNCTION_HOOK(sub_82BE7780, UnlockTextureRect);

GUEST_FUNCTION_HOOK(sub_82BE6B98, LockVertexBuffer);
GUEST_FUNCTION_HOOK(sub_82BE6BE8, UnlockVertexBuffer);
GUEST_FUNCTION_HOOK(sub_82BE61D0, GetVertexBufferDesc);

GUEST_FUNCTION_HOOK(sub_82BE6CA8, LockIndexBuffer);
GUEST_FUNCTION_HOOK(sub_82BE6CF0, UnlockIndexBuffer);
GUEST_FUNCTION_HOOK(sub_82BE6200, GetIndexBufferDesc);

GUEST_FUNCTION_HOOK(sub_82BE96F0, GetSurfaceDesc);

GUEST_FUNCTION_HOOK(sub_82BE04B0, GetVertexDeclaration);
GUEST_FUNCTION_HOOK(sub_82BE0530, HashVertexDeclaration);

GUEST_FUNCTION_HOOK(sub_82BDA8C0, Present);
GUEST_FUNCTION_HOOK(sub_82BDD330, GetBackBuffer);

GUEST_FUNCTION_HOOK(sub_82BE9498, CreateTexture);
GUEST_FUNCTION_HOOK(sub_82BE6AD0, CreateVertexBuffer);
GUEST_FUNCTION_HOOK(sub_82BE6BF8, CreateIndexBuffer);
GUEST_FUNCTION_HOOK(sub_82BE95B8, CreateSurface);

GUEST_FUNCTION_HOOK(sub_82BF6400, StretchRect);

GUEST_FUNCTION_HOOK(sub_82BDD9F0, SetRenderTarget);
GUEST_FUNCTION_HOOK(sub_82BDD2F0, GetDepthStencilSurface);
GUEST_FUNCTION_HOOK(sub_82BDDD38, SetDepthStencilSurface);

GUEST_FUNCTION_HOOK(sub_82BFE4C8, Clear);

GUEST_FUNCTION_HOOK(sub_82BDD8C0, SetViewport);
GUEST_FUNCTION_HOOK(sub_82BDD0A8, GetViewport);

GUEST_FUNCTION_HOOK(sub_82BE9818, SetTexture);
GUEST_FUNCTION_HOOK(sub_82BDCFB0, SetScissorRect);

GUEST_FUNCTION_HOOK(sub_82BE5900, DrawPrimitive);
GUEST_FUNCTION_HOOK(sub_82BE5CF0, DrawIndexedPrimitive);
GUEST_FUNCTION_HOOK(sub_82BE52F8, DrawPrimitiveUP);

GUEST_FUNCTION_HOOK(sub_82BE0428, CreateVertexDeclaration);
GUEST_FUNCTION_HOOK(sub_82BE02E0, SetVertexDeclaration);

GUEST_FUNCTION_HOOK(sub_82BE1A80, CreateVertexShader);
GUEST_FUNCTION_HOOK(sub_82BE0110, SetVertexShader);

GUEST_FUNCTION_HOOK(sub_82BDD0F8, SetStreamSource);
GUEST_FUNCTION_HOOK(sub_82BDD218, SetIndices);

GUEST_FUNCTION_HOOK(sub_82BE1990, CreatePixelShader);
GUEST_FUNCTION_HOOK(sub_82BDFE58, SetPixelShader);

GUEST_FUNCTION_HOOK(sub_82C00910, D3DXFillVolumeTexture);

GUEST_FUNCTION_HOOK(sub_82E43FC8, MakePictureData);

GUEST_FUNCTION_HOOK(sub_82E9EE38, SetResolution);
GUEST_FUNCTION_HOOK(sub_82BE77B0, StubFunction);

GUEST_FUNCTION_HOOK(sub_82AE2BF8, ScreenShaderInit);

GUEST_FUNCTION_STUB(sub_822C15D8);
GUEST_FUNCTION_STUB(sub_822C1810);
GUEST_FUNCTION_STUB(sub_82BD97A8);
GUEST_FUNCTION_STUB(sub_82BD97E8);
GUEST_FUNCTION_STUB(sub_82BDD370); // SetGammaRamp
GUEST_FUNCTION_STUB(sub_82BE05B8);
GUEST_FUNCTION_STUB(sub_82BE9C98);
GUEST_FUNCTION_STUB(sub_82BEA308);
GUEST_FUNCTION_STUB(sub_82CD5D68);
GUEST_FUNCTION_STUB(sub_82BE9B28);
GUEST_FUNCTION_STUB(sub_82BEA018);
GUEST_FUNCTION_STUB(sub_82BEA7C0);
GUEST_FUNCTION_STUB(sub_82BFFF88); // D3DXFilterTexture
GUEST_FUNCTION_STUB(sub_82E9EF90); // D3DXFillTexture
GUEST_FUNCTION_STUB(sub_82BD96D0);
