cmake_minimum_required(VERSION 3.16)
project(DockerHomelabManager VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Find CURL
find_package(CURL REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external
)

# Source files
set(SOURCES
    src/main.cpp
    src/ui/MainWindow.cpp
    src/docker/DockerClient.cpp
    src/docker/ContainerManager.cpp
    src/network/PortScanner.cpp
    src/network/NetworkDiagnostics.cpp
    src/storage/Database.cpp
    src/update/UpdateChecker.cpp
    src/diagnostics/ErrorDiagnostics.cpp
    src/utils/Logger.cpp
)

# Header files
set(HEADERS
    include/ui/MainWindow.h
    include/docker/DockerClient.h
    include/docker/ContainerManager.h
    include/docker/Container.h
    include/network/PortScanner.h
    include/network/NetworkDiagnostics.h
    include/storage/Database.h
    include/update/UpdateChecker.h
    include/diagnostics/ErrorDiagnostics.h
    include/utils/Logger.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    SQLite::SQLite3
    CURL::libcurl
)

# Installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# CPack configuration for packaging
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
