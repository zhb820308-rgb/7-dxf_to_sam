cmake_minimum_required(VERSION 3.5)
project(dxf_analyzer)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加可执行文件
add_executable(dxf_analyzer dxf_analyzer.cpp)

# 链接 libdxfrw 库
target_include_directories(dxf_analyzer PRIVATE "${CMAKE_SOURCE_DIR}/../src")
target_link_libraries(dxf_analyzer PRIVATE "${CMAKE_SOURCE_DIR}/../build/Release/dxfrw.lib")