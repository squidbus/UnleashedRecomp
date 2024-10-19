project("UnleashedRecompLib")

add_compile_options(
    "/fp:strict"
    "-march=sandybridge"
    "-fno-strict-aliasing")

file(GLOB SWA_PPC_RECOMPILED_SOURCES "ppc/*.cpp")
file(GLOB SWA_SHADER_RECOMPILED_SOURCES "shader/*.cpp")
add_library(UnleashedRecompLib "main.cpp" ${SWA_PPC_RECOMPILED_SOURCES} "shader/shader_cache.h" ${SWA_SHADER_RECOMPILED_SOURCES})

target_include_directories(UnleashedRecompLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(UnleashedRecompLib PUBLIC ppc/ppc_recomp_shared.h)

target_compile_definitions(PowerRecomp PRIVATE
    CONFIG_FILE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/config/SWA.toml\")

target_compile_definitions(ShaderRecomp PRIVATE 
    SHADER_RECOMP_INPUT=\"${CMAKE_CURRENT_SOURCE_DIR}/private\" 
    SHADER_RECOMP_OUTPUT=\"${CMAKE_CURRENT_SOURCE_DIR}/shader/shader_cache.cpp\")
