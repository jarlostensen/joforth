cmake_minimum_required(VERSION 3.18)

project(joForth)

add_executable(joForth joforth.c main.c "jo.h")

target_include_directories(joForth PUBLIC 
    "${CMAKE_SOURCE_DIR}"    
)