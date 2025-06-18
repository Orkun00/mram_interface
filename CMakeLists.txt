cmake_minimum_required(VERSION 3.31)
project(mram_interface C)

set(CMAKE_C_STANDARD 11)

add_library(mram_interface STATIC
        mram.c
        mram.h)
