cmake_minimum_required(VERSION 3.28)

project(XYLU LANGUAGES CXX)

# 加载 ccache
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    message(STATUS "ccache found: ${CCACHE_FOUND}, setting as compiler launcher.")
    set(CMAKE_CXX_LAUNCHER "${CCACHE_FOUND}")
    set(CMAKE_C_LAUNCHER "${CCACHE_FOUND}")
else()
    message(WARNING "ccache not found. Builds will not be cached.")
endif()

include_directories(D:/askii/include)
link_directories(D:/askii/lib/$<$<PLATFORM_ID:Windows>:win64>$<$<NOT:$<PLATFORM_ID:Windows>>:unix64>)

# ==========================================================
# 定义库 xylu
# ==========================================================
file(GLOB_RECURSE xylu_SOURCES "xylu/detail/*/__/*.cpp")
add_library(xylu STATIC ${xylu_SOURCES})

target_compile_features(xylu PUBLIC cxx_std_17)
target_compile_options(xylu PRIVATE -mavx2 -msse4.2)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(xylu PRIVATE -save-temps -ffile-prefix-map=${CMAKE_SOURCE_DIR}/=./)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fverbose-asm")
endif()

# ==========================================================
# 定义可执行文件 test
# ==========================================================
add_executable(test xylu/test.cpp)
target_compile_features(test PRIVATE cxx_std_17)
#target_compile_features(test PRIVATE cxx_std_23)
target_compile_options(test PRIVATE -mavx2 -msse4.2)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(test PRIVATE -ffile-prefix-map=${CMAKE_SOURCE_DIR}/=./)
endif()
target_link_libraries(test PRIVATE
        xylu
        $<$<PLATFORM_ID:Windows>:ws2_32 bcrypt crypt32>
        $<$<NOT:$<PLATFORM_ID:Windows>>:uring>
)

add_executable(test2 xylu/test2.cpp)
target_compile_features(test2 PRIVATE cxx_std_17)
target_compile_options(test2 PRIVATE -ffile-prefix-map=${CMAKE_SOURCE_DIR}/=./)
target_compile_options(test2 PRIVATE -mavx2 -msse4.2)
target_link_libraries(test2 PRIVATE
        xylu
        $<$<PLATFORM_ID:Windows>:ws2_32 bcrypt crypt32>
        $<$<NOT:$<PLATFORM_ID:Windows>>:uring>
)
