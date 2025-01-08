cmake_minimum_required(VERSION 3.12.4)
project(HistoryTest)

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
add_executable(history_publisher
    HistoryPublisher.cpp
    HistoryTest.cxx
    HistoryTestPubSubTypes.cxx)

add_executable(history_subscriber
    HistorySubscriber.cpp
    HistoryTest.cxx
    HistoryTestPubSubTypes.cxx)

# Link libraries
target_link_libraries(history_publisher
    fastrtps
    fastcdr
    Threads::Threads)

target_link_libraries(history_subscriber
    fastrtps
    fastcdr
    Threads::Threads)