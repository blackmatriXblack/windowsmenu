cmake_minimum_required(VERSION 4.0)
project(programs C)

set(CMAKE_C_STANDARD 11)

# 允许编译 Windows 资源文件
if(WIN32)
    set(CMAKE_RC_COMPILER windres)
    set(RESOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/resource.rc)
    # 生成 .o 目标文件
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/resource.o
            COMMAND windres ${RESOURCE_FILE} -o ${CMAKE_CURRENT_BINARY_DIR}/resource.o
            DEPENDS ${RESOURCE_FILE}
    )
    set(EXTRA_RESOURCES ${CMAKE_CURRENT_BINARY_DIR}/resource.o)
endif()

add_executable(programs main.c
        resource.rc ${EXTRA_RESOURCES})
