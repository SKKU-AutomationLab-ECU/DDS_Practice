cmake_minimum_required(VERSION 3.12.4)
project(ReliabilityTest)

# Find requirements
find_package(FastRTPS REQUIRED)
find_package(FastCDR REQUIRED)
find_package(Threads REQUIRED)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FastRTPS_INCLUDE_DIR}
)

# Create executables
add_executable(reliability_publisher
    ReliabilityPublisher.cpp
    ReliabilityTest.cxx
    ReliabilityTestPubSubTypes.cxx)

add_executable(reliability_subscriber
    ReliabilitySubscriber.cpp
    ReliabilityTest.cxx
    ReliabilityTestPubSubTypes.cxx)

# Link libraries
target_link_libraries(reliability_publisher
    fastrtps
    fastcdr
    Threads::Threads)

target_link_libraries(reliability_subscriber
    fastrtps
    fastcdr
    Threads::Threads)