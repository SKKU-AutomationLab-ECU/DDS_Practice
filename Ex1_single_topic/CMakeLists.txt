cmake_minimum_required(VERSION 3.12.4)

# Set project name
project(vehicle_diagnostics VERSION 1.0.0 LANGUAGES CXX)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(FastRTPS REQUIRED)
find_package(fastcdr REQUIRED)
find_package(Threads REQUIRED)  # 추가된 부분

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FastRTPS_INCLUDE_DIR}
)

# Add publisher executable
add_executable(vehicle_publisher
    VehicleDiagnosticsPublisher.cpp
    VehicleDiagnostics.cxx
    VehicleDiagnosticsPubSubTypes.cxx
)

# Add subscriber executable
add_executable(vehicle_subscriber
    VehicleDiagnosticsSubscriber.cpp
    VehicleDiagnostics.cxx
    VehicleDiagnosticsPubSubTypes.cxx
)

target_link_libraries(vehicle_publisher 
    fastrtps 
    fastcdr
    Threads::Threads)  # pthread 링크
    
target_link_libraries(vehicle_subscriber 
    fastrtps 
    fastcdr
    Threads::Threads)  # pthread 링크