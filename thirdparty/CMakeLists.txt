include(FetchContent)

FetchContent_Declare(
    unordered_dense
    GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
    GIT_TAG        main
)
FetchContent_Declare(
    xxHash
    GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
    GIT_TAG        v0.8.2
    SOURCE_SUBDIR "cmake_unofficial"
)
FetchContent_MakeAvailable(unordered_dense)
FetchContent_MakeAvailable(xxHash)

add_subdirectory(${SWA_THIRDPARTY_ROOT}/PowerRecomp)
add_subdirectory(${SWA_THIRDPARTY_ROOT}/o1heap)