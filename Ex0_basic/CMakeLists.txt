cmake_minimum_required(VERSION 3.12.4)
project(HelloWorld)

# Find requirements
find_package(FastRTPS REQUIRED)
find_package(FastCDR REQUIRED)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)

# Include current directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Create executables
add_executable(publisher 
    HelloWorldPublisher.cpp 
    HelloWorld.cxx 
    HelloWorldPubSubTypes.cxx)
    
add_executable(subscriber 
    HelloWorldSubscriber.cpp 
    HelloWorld.cxx 
    HelloWorldPubSubTypes.cxx)

# Link libraries
target_link_libraries(publisher fastrtps fastcdr)
target_link_libraries(subscriber fastrtps fastcdr)