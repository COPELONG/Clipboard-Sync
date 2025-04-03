cmake_minimum_required(VERSION 3.10)
add_definitions(-D_WIN32_WINNT=0x0A00)  # Windows 10
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(WebSocket)

# 设置可执行文件输出目录为项目根目录下的 bin 文件夹
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 设置 Boost 头文件的包含路径
include_directories(. boost)

# 设置源文件
set(SOURCES server.cpp)

#set(SOURCE ppp.cpp)
# 创建可执行文件
#add_executable(socket ${SOURCES})

add_executable(server ${SOURCES})
target_link_libraries(server PRIVATE ws2_32)
