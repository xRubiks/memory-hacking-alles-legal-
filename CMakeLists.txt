cmake_minimum_required(VERSION 3.31)
project(c___playground)

set(CMAKE_CXX_STANDARD 20)

# Konsolen-Version
add_executable(memory_scanner_console
    main.cpp
    src/process_utils.cpp
    src/process_utils.h
    src/memory_scanner.cpp
    src/memory_scanner.h
)

# GUI-Version (ohne Konsolenfenster)
add_executable(memory_scanner_gui WIN32
    src/gui_main.cpp
    src/process_utils.cpp
    src/process_utils.h
    src/memory_scanner.cpp
    src/memory_scanner.h
)
target_link_libraries(memory_scanner_gui comctl32)

# Standard-Target für CLion
add_executable(c___playground
    src/gui_main.cpp
    src/process_utils.cpp
    src/process_utils.h
    src/memory_scanner.cpp
    src/memory_scanner.h
)
set_target_properties(c___playground PROPERTIES WIN32_EXECUTABLE TRUE)
target_link_libraries(c___playground comctl32)
