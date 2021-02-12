cmake_minimum_required(VERSION 3.18)

project(joForth)

set(CMAKE_C_STANDARD 11)
add_executable(joForth joforth.c main.c "jo.h" "joforth_ir.h")

target_include_directories(joForth PUBLIC 
    "${CMAKE_SOURCE_DIR}"    
)