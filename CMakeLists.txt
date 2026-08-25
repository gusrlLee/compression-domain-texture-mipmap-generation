cmake_minimum_required(VERSION 3.30)

project(compression_domain_mipmap_generation)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT IS_DIRECTORY $ENV{VULKAN_SDK})
    message(WARNING "Failed to find Vulkan SDK.")
else()
    message(STATUS "Vulkan SDK path: $ENV{VULKAN_SDK}")
endif()

include_directories(SYSTEM
    $ENV{VULKAN_SDK}/include
    $ENV{VULKAN_SDK}/include/slang)

find_package(Vulkan REQUIRED)
find_library(
    Slang_LIBRARY
    NAMES slang slang-compiler
    HINTS "$ENV{VULKAN_SDK}/lib"
    REQUIRED
)

add_executable(
    ${PROJECT_NAME} 
    src/main.cpp
    src/type.h
    src/dds_loader.h
    src/dds_loader.cpp
)

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
        ${Slang_LIBRARY}
        Vulkan::Vulkan
)