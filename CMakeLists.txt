cmake_minimum_required(VERSION 3.16)
project(DockerHomelabManager VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# FetchContent for dependencies
include(FetchContent)

# Fetch nlohmann/json
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

# Fetch yaml-cpp
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG yaml-cpp-0.7.0
)
FetchContent_MakeAvailable(yaml-cpp)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

# Find SQLite3 (using unofficial package from vcpkg)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

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
    src/ui/LogsViewerDialog.cpp
    src/docker/DockerClient.cpp
    src/docker/ContainerManager.cpp
    src/docker/DependencyResolver.cpp
    src/compose/ComposeParser.cpp
    src/compose/ComposeStack.cpp
    src/registry/RegistryManager.cpp
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
    include/ui/LogsViewerDialog.h
    include/ui/SettingsDialog.h
    include/docker/DockerClient.h
    include/docker/ContainerManager.h
    include/docker/Container.h
    include/docker/DependencyResolver.h
    include/compose/ComposeParser.h
    include/compose/ComposeStack.h
    include/registry/RegistryManager.h
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
    unofficial::sqlite3::sqlite3
    CURL::libcurl
    nlohmann_json::nlohmann_json
    yaml-cpp
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32 iphlpapi)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} pthread)
endif()

# Installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# CPack configuration for packaging
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
