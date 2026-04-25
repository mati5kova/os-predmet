cmake_minimum_required(VERSION 4.1)
project(sheila C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(sheila
        main.c)