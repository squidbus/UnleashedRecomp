project("UnleashedRecomp")
set(TARGET_NAME "SWA")
file(GLOB "*.cpp")

add_compile_definitions(SWA_IMPL)
add_compile_options(
    "/D_HAS_EXCEPTIONS=0"
    "/fp:strict"
    "/GS-"
    "/EHa-"
    "-march=haswell"
    "-fno-strict-aliasing")


file(GLOB SWA_RECOMPILED_SOURCES "ppc/*.cpp")

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
)

set(SWA_HID_CXX_SOURCES
    "hid/hid.cpp"
)

set(SWA_CXX_SOURCES
    "main.cpp"

    ${SWA_KERNEL_CXX_SOURCES}
    ${SWA_CPU_CXX_SOURCES}
    ${SWA_GPU_CXX_SOURCES}
    ${SWA_HID_CXX_SOURCES}
)

add_executable(UnleashedRecomp ${SWA_RECOMPILED_SOURCES} ${SWA_CXX_SOURCES})
set_target_properties(UnleashedRecomp PROPERTIES OUTPUT_NAME ${TARGET_NAME})

target_link_libraries(UnleashedRecomp PUBLIC 
    PowerUtils 
    o1heap 
    xxHash::xxhash 
    unordered_dense::unordered_dense 
    winmm
    ntdll
    comctl32
)
target_include_directories(UnleashedRecomp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_precompile_headers(UnleashedRecomp PUBLIC "ppc/ppc_recomp_shared.h")
