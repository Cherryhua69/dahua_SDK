cmake_minimum_required(VERSION 3.10)
 
project(dahua_IVD)
 
# 设置C++标准
set(CMAKE_CXX_STANDARD 14)
 
# 找到OpenCV库
find_package(OpenCV REQUIRED)
 
# 输出OpenCV库的信息
message(STATUS "OpenCV library status:")
message(STATUS "    config: ${OpenCV_DIR}")
message(STATUS "    version: ${OpenCV_VERSION}")
message(STATUS "    libraries: ${OpenCV_LIBS}")
message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")
 
# 找到FFmpeg库
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswresample libswscale libavfilter)
 
# 输出FFmpeg库的信息
message(STATUS "FFmpeg library status:")
message(STATUS "    include path: ${FFMPEG_INCLUDE_DIRS}")
message(STATUS "    libraries: ${FFMPEG_LIBRARIES}")
 
# 添加头文件路径到编译器的头文件搜索路径下
include_directories(${OpenCV_INCLUDE_DIRS} ${FFMPEG_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/include)
 
# 添加可执行文件
add_executable(Yulan test.cpp)  
 
# 链接库文件
target_link_libraries(Yulan PRIVATE 
    ${FFMPEG_LIBRARIES}
    ${OpenCV_LIBS}
    /home/hf/projects/dahua_IVD/libs/libavnetsdk.so
    /home/hf/projects/dahua_IVD/libs/libdhconfigsdk.so
    /home/hf/projects/dahua_IVD/libs/libStreamConvertor.so
    /home/hf/projects/dahua_IVD/libs/libdhnetsdk.so
    /home/hf/projects/dahua_IVD/libs/libplay.so
    # libRenderEngine.so这个动态库需要添加到系统环境中：export LD_LIBRARY_PATH=/home/hf/projects/dahua_IVD/libs:$LD_LIBRARY_PATH
    /home/hf/projects/dahua_IVD/libs/libRenderEngine.so
    jsoncpp
)