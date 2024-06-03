#TODO 1: 要求的cmake最低版本
cmake_minimum_required(VERSION 3.10)
set(CMAKE_C_COMPILER gcc)

#TODO 2: 创建一个名为chip8的项目
project(Chip8_Emulator)

#TODO 3: 设置生成的可执行文件的保存路径
set (EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
#1.SDL2所在目录
set(SDL2_DIR Y:/Coding/.lib/SDL2-devel-2.30.3-mingw/SDL2-2.30.3/x86_64-w64-mingw32)
#2.SDL2 Image所在目录
set(SDL2IMAGE_DIR Y:/Coding/.lib/SDL2_image-devel-2.8.2-mingw/SDL2_image-2.8.2/x86_64-w64-mingw32)
#3.SDL2 ttf所在目录
set(SDL2TTF_DIR Y:/Coding/.lib/SDL2_ttf-devel-2.22.0-mingw/SDL2_ttf-2.22.0/x86_64-w64-mingw32)
#4.SDL2 mixer所在目录
set(SDL2MIXER_DIR Y:/Coding/.lib/SDL2_mixer-devel-2.8.0-mingw/SDL2_mixer-2.8.0/x86_64-w64-mingw32)

#TODO 4: 将源码文件的路径保存在SRC_LIST变量中
aux_source_directory(src SRC_LIST)

#TODO 5: 指定头文件路径
include_directories(include)
include_directories(${SDL2_DIR}/include/SDL2)
include_directories(${SDL2IMAGE_DIR}/include/SDL2)
include_directories(${SDL2TTF_DIR}/include/SDL2)
include_directories(${SDL2MIXER_DIR}/include/SDL2)

#SDL2库目录
link_directories(${SDL2_DIR}/lib)
link_directories(${SDL2IMAGE_DIR}/lib)
link_directories(${SDL2TTF_DIR}/lib)
link_directories(${SDL2MIXER_DIR}/lib)

# 库
link_libraries(mingw32 SDL2main SDL2 SDL2_image SDL2_ttf SDL2_mixer)

#TODO 6: 生成可执行文件
add_executable(chip8 ${SRC_LIST})