cmake_minimum_required(VERSION 3.31)
project(SwaggTransformer)

set(CMAKE_CXX_STANDARD 20)

include_directories(headers)

add_executable(SwaggTransformer
        headers/embedding.h
        headers/tokenizer.h
        source/main.cpp)
