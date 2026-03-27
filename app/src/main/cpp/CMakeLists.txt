cmake_minimum_required(VERSION 3.10)
project(kail_gait_sim C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(kail_gait_sim SHARED
    simulate.cpp
    config.cpp
    gait_jni.cpp
)

target_include_directories(kail_gait_sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_library(log-lib log)

find_package(dobby CONFIG REQUIRED)
target_link_libraries(kail_gait_sim PRIVATE ${log-lib} dobby::dobby)
