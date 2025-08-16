cmake_minimum_required(VERSION 3.31)
project(SwaggGPT)

set(CMAKE_CXX_STANDARD 20)

include_directories(headers)

add_executable(SwaggGPT
        headers/embedding.h
        source/main.cpp
        headers/tokenizer.h)
