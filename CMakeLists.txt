cmake_minimum_required(VERSION 4.1)
project(cfer C)

set(CMAKE_C_STANDARD 17)

add_executable(cfer main.c
        common.h
        chunk.h
        chunk.c
        memory.h
        memory.c
        debug.h
        debug.c
        value.c
        value.h
        vm.c
        vm.h
        compiler.c
        compiler.h
        scanner.c
        scanner.h)
