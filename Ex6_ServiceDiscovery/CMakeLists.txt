cmake_minimum_required(VERSION 3.12.4)
project(ServiceDiscovery)

# Find requirements
if(NOT fastcdr_FOUND)
    find_package(fastcdr REQUIRED)
endif()

if(NOT foonathan_memory_FOUND)
    find_package(foonathan_memory REQUIRED)
endif()

if(NOT FastRTPS_FOUND)
    find_package(FastRTPS REQUIRED)
endif()

find_package(Threads REQUIRED)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FastRTPS_INCLUDE_DIR}
    ${fastcdr_INCLUDE_DIR}
)

# Create executables
add_executable(service_publisher
    ServiceDiscoveryPublisher.cpp
    ServiceDiscovery.cxx
    ServiceDiscoveryPubSubTypes.cxx)

add_executable(service_subscriber
    ServiceDiscoverySubscriber.cpp
    ServiceDiscovery.cxx
    ServiceDiscoveryPubSubTypes.cxx)

# Link libraries
target_link_libraries(service_publisher
    fastrtps
    fastcdr
    foonathan_memory
    Threads::Threads)

target_link_libraries(service_subscriber
    fastrtps
    fastcdr
    foonathan_memory
    Threads::Threads)