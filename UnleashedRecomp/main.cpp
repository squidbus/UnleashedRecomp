#include <stdafx.h>
#include <cpu/code_cache.h>
#include <cpu/guest_thread.h>
#include <gpu/video.h>
#include <kernel/function.h>
#include <kernel/memory.h>
#include <kernel/heap.h>
#include <kernel/xam.h>
#include <kernel/io/file_system.h>
#include <file.h>
#include <xex.h>
#include <apu/audio.h>

#define GAME_XEX_PATH "game:\\default.xex"

const size_t XMAIOBegin = 0x7FEA0000;
const size_t XMAIOEnd = XMAIOBegin + 0x0000FFFF;

Memory gMemory{ reinterpret_cast<void*>(0x100000000), 0x100000000 };
Heap gUserHeap;
CodeCache gCodeCache;

// Name inspired from nt's entry point
void KiSystemStartup()
{
#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif

    gMemory.Alloc(0x10000, 0x1000, MEM_COMMIT);
    gUserHeap.Init();
    gCodeCache.Init();

    gMemory.Alloc(XMAIOBegin, 0xFFFF, MEM_COMMIT);

    const auto gameContent = XamMakeContent(XCONTENTTYPE_RESERVED, "Game");
    const auto updateContent = XamMakeContent(XCONTENTTYPE_RESERVED, "Update");
    XamRegisterContent(gameContent, DirectoryExists(".\\game") ? ".\\game" : ".");
    XamRegisterContent(updateContent, ".\\update");

    if (FileExists(".\\save\\SYS-DATA"))
    {
        XamRegisterContent(XamMakeContent(XCONTENTTYPE_SAVEDATA, "SYS-DATA"), ".\\save");
    }
    else if (FileExists(".\\SYS-DATA"))
    {
        XamRegisterContent(XamMakeContent(XCONTENTTYPE_SAVEDATA, "SYS-DATA"), ".");
    }

    // Mount game
    XamContentCreateEx(0, "game", &gameContent, OPEN_EXISTING, nullptr, nullptr, 0, 0, nullptr);
    XamContentCreateEx(0, "update", &updateContent, OPEN_EXISTING, nullptr, nullptr, 0, 0, nullptr);

    // OS mounts game data to D:
    XamContentCreateEx(0, "D", &gameContent, OPEN_EXISTING, nullptr, nullptr, 0, 0, nullptr);

    XAudioInitializeSystem();
}

uint32_t LdrLoadModule(const char* path)
{
    auto loadResult = LoadFile(FileSystem::TransformPath(GAME_XEX_PATH));
    if (!loadResult.has_value())
    {
        assert("Failed to load module" && false);
        return 0;
    }

    auto* xex = reinterpret_cast<XEX_HEADER*>(loadResult->data());
    auto security = reinterpret_cast<XEX2_SECURITY_INFO*>((char*)xex + xex->AddressOfSecurityInfo);

    gMemory.Alloc(security->ImageBase, security->SizeOfImage, MEM_COMMIT);

    auto format = Xex2FindOptionalHeader<XEX_FILE_FORMAT_INFO>(xex, XEX_HEADER_FILE_FORMAT_INFO);
    auto entry = *Xex2FindOptionalHeader<uint32_t>(xex, XEX_HEADER_ENTRY_POINT);
    ByteSwap(entry);
    assert(format->CompressionType >= 1);

    if (format->CompressionType == 1)
    {
        auto srcData = (char*)xex + xex->SizeOfHeader;
        auto destData = (char*)gMemory.Translate(security->ImageBase);

        auto numBlocks = (format->SizeOfHeader / sizeof(XEX_BASIC_FILE_COMPRESSION_INFO)) - 1;
        auto blocks = reinterpret_cast<const XEX_BASIC_FILE_COMPRESSION_INFO*>(format + 1);

        for (size_t i = 0; i < numBlocks; i++)
        {
            memcpy(destData, srcData, blocks[i].SizeOfData);

            srcData += blocks[i].SizeOfData;
            destData += blocks[i].SizeOfData;
            memset(destData, 0, blocks[i].SizeOfPadding);

            destData += blocks[i].SizeOfPadding;
        }
    }

    return entry;
}

int main()
{
    KiSystemStartup();

    uint32_t entry = LdrLoadModule(FileSystem::TransformPath(GAME_XEX_PATH));

    VdInitializeSystem();

    GuestThread::Start(entry);

    return 0;
}

GUEST_FUNCTION_STUB(__imp__vsprintf);
GUEST_FUNCTION_STUB(__imp___vsnprintf);
GUEST_FUNCTION_STUB(__imp__sprintf);
GUEST_FUNCTION_STUB(__imp___snprintf);
GUEST_FUNCTION_STUB(__imp___snwprintf);
GUEST_FUNCTION_STUB(__imp__vswprintf);
GUEST_FUNCTION_STUB(__imp___vscwprintf);
GUEST_FUNCTION_STUB(__imp__swprintf);
