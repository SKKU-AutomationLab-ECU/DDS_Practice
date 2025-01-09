cmake_minimum_required(VERSION 3.12.4)
project(SteeringControl)

# Find requirements
find_package(FastRTPS REQUIRED)
find_package(FastCDR REQUIRED)
find_package(Threads REQUIRED)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FastRTPS_INCLUDE_DIR}
)

# Create executables
add_executable(steering_publisher
    SteeringPublisher.cpp
    SteeringControl.cxx
    SteeringControlPubSubTypes.cxx)

add_executable(steering_subscriber
    SteeringSubscriber.cpp
    SteeringControl.cxx
    SteeringControlPubSubTypes.cxx)

# Link libraries
target_link_libraries(steering_publisher
    fastrtps
    fastcdr
    Threads::Threads)

target_link_libraries(steering_subscriber
    fastrtps
    fastcdr
    Threads::Threads)