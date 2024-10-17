project("UnleashedRecomp")
set(TARGET_NAME "SWA")

add_compile_definitions(SWA_IMPL)
add_compile_definitions(SDL_MAIN_HANDLED)

# Microsoft wtf?
add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

BIN2H(SOURCE_FILE "res/icon.bmp" HEADER_FILE "res/icon.h" ARRAY_TYPE "unsigned char" VARIABLE_NAME "g_icon")

set(SWA_PRECOMPILED_HEADERS
    "stdafx.h"
)

set(SWA_KERNEL_CXX_SOURCES
    "kernel/imports.cpp"
    "kernel/xdm.cpp"
    "kernel/heap.cpp"
    "kernel/memory.cpp"
    "kernel/xam.cpp"
    "kernel/io/file_system.cpp"
)

set(SWA_CPU_CXX_SOURCES
    "cpu/guest_thread.cpp"
    "cpu/code_cache.cpp"
)

set(SWA_GPU_CXX_SOURCES
    "gpu/video.cpp"
    "gpu/rhi/rt64_d3d12.cpp"
    "gpu/rhi/rt64_vulkan.cpp"
)

set(SWA_APU_CXX_SOURCES
    "apu/audio.cpp"
    "apu/driver/xaudio_driver.cpp"
)

set(SWA_HID_CXX_SOURCES
    "hid/hid.cpp"
    "hid/driver/sdl_hid.cpp"
)

set(SWA_UI_CXX_SOURCES
    "ui/window.cpp"
)

set(SWA_CXX_SOURCES
    "game.cpp"
    "main.cpp"
    "misc_impl.cpp"

    ${SWA_KERNEL_CXX_SOURCES}
    ${SWA_CPU_CXX_SOURCES}
    ${SWA_GPU_CXX_SOURCES}
    ${SWA_APU_CXX_SOURCES}
    ${SWA_HID_CXX_SOURCES}
    ${SWA_UI_CXX_SOURCES}
)

add_executable(UnleashedRecomp ${SWA_CXX_SOURCES})
set_target_properties(UnleashedRecomp PROPERTIES OUTPUT_NAME ${TARGET_NAME})

find_package(d3d12-memory-allocator CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(unordered_dense CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG)
find_package(volk CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(xxHash CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(tomlplusplus REQUIRED IMPORTED_TARGET tomlplusplus)

target_link_libraries(UnleashedRecomp PRIVATE
    comctl32
    d3d12
    dxgi
    Vulkan::Headers
    volk::volk 
    volk::volk_headers
    GPUOpen::VulkanMemoryAllocator
    ntdll
    o1heap
    PowerUtils
    SDL2::SDL2-static
    UnleashedRecompLib
    unofficial::D3D12MemoryAllocator
    unordered_dense::unordered_dense
    winmm
    xxHash::xxhash
    PkgConfig::tomlplusplus
)

target_include_directories(UnleashedRecomp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${SWA_THIRDPARTY_ROOT}/ddspp 
)

target_precompile_headers(UnleashedRecomp PUBLIC ${SWA_PRECOMPILED_HEADERS})
