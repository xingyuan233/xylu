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

# ==========================================================
# 定义库 xylu
# ==========================================================
file(GLOB_RECURSE xylu_SOURCES "src/*/*.cpp")
add_library(xylu STATIC ${xylu_SOURCES})

target_compile_features(xylu PUBLIC cxx_std_17)
target_compile_options(xylu PRIVATE -mavx2 -msse4.2)
