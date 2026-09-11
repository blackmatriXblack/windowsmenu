cmake_minimum_required(VERSION 3.16)
project(WindowsControlQt LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(WindowsControl-Qt WIN32
    main.cpp
    qtsupport.c
)

set_source_files_properties(qtsupport.c PROPERTIES LANGUAGE C)

target_include_directories(WindowsControl-Qt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(WindowsControl-Qt PRIVATE Qt6::Widgets crypt32)

target_compile_options(WindowsControl-Qt PRIVATE /utf-8)