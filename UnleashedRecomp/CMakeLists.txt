project("UnleashedRecomp")
set(TARGET_NAME "SWA")

add_compile_definitions(SWA_IMPL)
add_compile_definitions(SDL_MAIN_HANDLED)

# Microsoft wtf?
add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

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
    "gpu/window.cpp"
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

set(SWA_CXX_SOURCES
    "main.cpp"
    "misc_impl.cpp"

    ${SWA_KERNEL_CXX_SOURCES}
    ${SWA_CPU_CXX_SOURCES}
    ${SWA_GPU_CXX_SOURCES}
    ${SWA_APU_CXX_SOURCES}
    ${SWA_HID_CXX_SOURCES}
)

add_executable(UnleashedRecomp ${SWA_CXX_SOURCES})
set_target_properties(UnleashedRecomp PROPERTIES OUTPUT_NAME ${TARGET_NAME})

target_link_libraries(UnleashedRecomp PUBLIC 
    UnleashedRecompLib
    PowerUtils
    o1heap
    xxHash::xxhash
    unordered_dense::unordered_dense
    SDL2::SDL2-static
    winmm
    ntdll
    comctl32
    d3d12
    dxgi
)
target_include_directories(UnleashedRecomp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${SWA_THIRDPARTY_ROOT}/ddspp 
    ${SWA_THIRDPARTY_ROOT}/D3D12MemoryAllocator/include
    ${SWA_THIRDPARTY_ROOT}/D3D12MemoryAllocator/src
    ${SWA_THIRDPARTY_ROOT}/volk
    ${SWA_THIRDPARTY_ROOT}/Vulkan-Headers/include
    ${SWA_THIRDPARTY_ROOT}/VulkanMemoryAllocator/include
    ${SWA_THIRDPARTY_ROOT}/VulkanMemoryAllocator/src
)

target_precompile_headers(UnleashedRecomp PUBLIC ${SWA_PRECOMPILED_HEADERS})
