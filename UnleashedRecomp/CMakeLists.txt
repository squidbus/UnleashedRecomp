project("UnleashedRecomp")
set(TARGET_NAME "SWA")

add_compile_options(
    "/fp:strict"
    "-march=sandybridge"
    "-fno-strict-aliasing")

add_compile_definitions(
    SWA_IMPL
    SDL_MAIN_HANDLED
    _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR # Microsoft wtf?
    _CRT_SECURE_NO_WARNINGS)

# Set config paths.
set(SWA_CONFIG_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/res/config.toml")
set(SWA_CONFIG_TARGET_PATH "${CMAKE_BINARY_DIR}/${PROJECT_NAME}")
set(SWA_CONFIG_TARGET_NAME "${TARGET_NAME}.toml")
set(SWA_CONFIG_TARGET_FILE "${SWA_CONFIG_TARGET_PATH}/${SWA_CONFIG_TARGET_NAME}")

# Copy default config to build directory if newer.
if (${SWA_CONFIG_SOURCE_FILE} IS_NEWER_THAN ${SWA_CONFIG_TARGET_FILE})
    file(INSTALL
        DESTINATION ${SWA_CONFIG_TARGET_PATH}
        TYPE FILE
        FILES ${SWA_CONFIG_SOURCE_FILE}
        RENAME ${SWA_CONFIG_TARGET_NAME}
    )
endif()

# Generate icon bitmap header for SDL surface.
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
    "config.cpp"
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

if (WIN32)
    # Set up Win32 resources for application icon.
    set(ICON_PATH "${PROJECT_SOURCE_DIR}/res/win32/icon.ico")
    configure_file("res/win32/res.rc" "${CMAKE_BINARY_DIR}/res.rc" @ONLY)
    add_executable(UnleashedRecomp ${SWA_CXX_SOURCES} "${CMAKE_BINARY_DIR}/res.rc")
else()
    add_executable(UnleashedRecomp ${SWA_CXX_SOURCES})
endif()

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
find_package(directx-dxc REQUIRED)
find_package(zstd CONFIG REQUIRED)

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
    zstd::libzstd_static
)

target_include_directories(UnleashedRecomp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${SWA_THIRDPARTY_ROOT}/ddspp 
)

target_precompile_headers(UnleashedRecomp PUBLIC ${SWA_PRECOMPILED_HEADERS})

function(compile_shader FILE_PATH TARGET_NAME)
    set(FILE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/gpu/shader/${FILE_PATH}.hlsl)
    cmake_path(GET FILE_PATH STEM VARIABLE_NAME)
    add_custom_command(
        OUTPUT ${FILE_PATH}.dxil.h
        COMMAND ${DIRECTX_DXC_TOOL} -T ${TARGET_NAME} -HV 2021 -all-resources-bound -Wno-ignored-attributes -Fh ${FILE_PATH}.dxil.h ${FILE_PATH} -Vn g_${VARIABLE_NAME}_dxil
        DEPENDS ${FILE_PATH}
    )
    add_custom_command(
        OUTPUT ${FILE_PATH}.spirv.h
        COMMAND ${DIRECTX_DXC_TOOL} -T ${TARGET_NAME} -HV 2021 -all-resources-bound -spirv -fvk-use-dx-layout ${ARGN} -Fh ${FILE_PATH}.spirv.h ${FILE_PATH} -Vn g_${VARIABLE_NAME}_spirv
        DEPENDS ${FILE_PATH}
    )
    target_sources(UnleashedRecomp PRIVATE ${FILE_PATH}.dxil.h ${FILE_PATH}.spirv.h)
endfunction()

function(compile_vertex_shader FILE_PATH)
    compile_shader(${FILE_PATH} vs_6_0 -fvk-invert-y)
endfunction()

function(compile_pixel_shader FILE_PATH)
    compile_shader(${FILE_PATH} ps_6_0)
endfunction()

compile_vertex_shader(copy_vs)
compile_pixel_shader(resolve_msaa_depth_2x)
compile_pixel_shader(resolve_msaa_depth_4x)
compile_pixel_shader(resolve_msaa_depth_8x)
