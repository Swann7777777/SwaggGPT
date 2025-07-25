cmake_minimum_required(VERSION 3.31)
project(SwaggTransformer)

set(CMAKE_CXX_STANDARD 20)

add_executable(SwaggTransformer source/main.cpp
        headers/tokenizer.h
        headers/embedding.h)
