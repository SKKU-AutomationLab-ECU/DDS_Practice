cmake_minimum_required(VERSION 3.12.4)

project(EX7_Domain)

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

# Generate code from IDL
file(GLOB IDL_FILES "*.idl")
foreach(IDL ${IDL_FILES})
    get_filename_component(IDL_NAME ${IDL} NAME_WE)
    fastrtps_idl(${IDL_NAME}_GEN "${IDL}")
endforeach()

# Create executables
add_executable(domain_publisher 
    DomainPublisher.cpp
    Domain.cxx
    DomainPubSubTypes.cxx
    DomainCdrAux.cpp
)

add_executable(domain_subscriber
    DomainSubscriber.cpp
    Domain.cxx
    DomainPubSubTypes.cxx
    DomainCdrAux.cpp
)

# Link libraries
target_link_libraries(domain_publisher
    fastrtps
    fastcdr
    Threads::Threads
)

target_link_libraries(domain_subscriber
    fastrtps
    fastcdr
    Threads::Threads
)