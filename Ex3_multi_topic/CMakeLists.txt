cmake_minimum_required(VERSION 3.12.4)
project(VehicleSystems)

# Find requirements
find_package(FastRTPS REQUIRED)
find_package(FastCDR REQUIRED)
find_package(Threads REQUIRED)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)

# Include current directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Create executables
add_executable(vehicle_publisher
    VehicleSystemsPublisher.cpp
    VehicleSystems.cxx
    VehicleSystemsPubSubTypes.cxx)

add_executable(vehicle_subscriber
    VehicleSystemsSubscriber.cpp
    VehicleSystems.cxx
    VehicleSystemsPubSubTypes.cxx)

# Link libraries
target_link_libraries(vehicle_publisher 
    fastrtps 
    fastcdr
    Threads::Threads)

target_link_libraries(vehicle_subscriber 
    fastrtps 
    fastcdr
    Threads::Threads)