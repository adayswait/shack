# CMakeList.txt: shack 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。

project(shack)

cmake_minimum_required (VERSION 3.8)
if(CMAKE_COMPILER_IS_GNUC)
    SET (CMAKE_C_FLAGS "-I. -O2 -g -ldl -lm -Wl,-export-dynamic")
    add_definitions(-DWITH_MAIN)
else(CMAKE_COMPILER_IS_GNUC)
    SET (CMAKE_C_FLAGS "-I. /Ot /GL /GS /Zi")
    add_definitions(-DWITH_MAIN)
endif(CMAKE_COMPILER_IS_GNUC)

# 将源代码添加到此项目的可执行文件。
add_executable (shack "shack.c" "shack.h")

# TODO: 如有需要，请添加测试并安装目标。