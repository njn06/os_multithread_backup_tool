cmake_minimum_required(VERSION 3.10)
project(AdvancedBackupTool)

set(CMAKE_CXX_STANDARD 17)

# Include header files
include_directories(include)

# Find dependencies
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Qt5 COMPONENTS Widgets REQUIRED)

# Shared sources (no entry points)
set(SHARED_SOURCES
    src/backup.cpp
    src/compress.cpp
    src/encrypt.cpp
    src/logger.cpp
)

# CLI target
add_executable(AdvancedBackupTool
    ${SHARED_SOURCES}
    src/main.cpp
)
target_link_libraries(AdvancedBackupTool PRIVATE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB pthread)

# GUI target
add_executable(AdvancedBackupToolGUI
    ${SHARED_SOURCES}
    src/gui.cpp
)
target_link_libraries(AdvancedBackupToolGUI PRIVATE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB Qt5::Widgets pthread)
