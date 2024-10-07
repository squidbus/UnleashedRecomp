project("UnleashedRecompLib")

add_compile_options(
    "/fp:strict"
    "-march=sandybridge"
    "-fno-strict-aliasing")

file(GLOB SWA_RECOMPILED_SOURCES "ppc/*.cpp")
add_library(UnleashedRecompLib ${SWA_RECOMPILED_SOURCES})

target_include_directories(UnleashedRecompLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(UnleashedRecompLib PUBLIC ppc/ppc_recomp_shared.h)
target_compile_definitions(PowerRecomp PRIVATE CONFIG_FILE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/config/SWA.toml\")
