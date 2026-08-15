cmake_minimum_required(VERSION 3.10)
project(WindowsControlCenter C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "This project is Windows-only (pure Win32 API).")
endif()

add_executable(WindowsControl WIN32 windowscontrol.c)

target_link_libraries(WindowsControl
    shell32
    advapi32
    user32
    powrprof
    ole32
    uuid
    gdi32
    winmm
    comdlg32
    ws2_32
    winhttp
    iphlpapi
    crypt32
)

set_target_properties(WindowsControl PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)