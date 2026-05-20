cmake_minimum_required(VERSION 3.16)
project(GCS_Autopilot CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -pthread)

set(NETWORK_SOURCES
    src/network_layer/TcpSocket.cpp
    src/network_layer/UdpSocket.cpp
    src/network_layer/NetworkLayer.cpp)

set(FLIGHT_SOURCES
    src/flight_logic/FlightLogic.cpp)

add_executable(gcs
    src/gcs_main.cpp
    ${NETWORK_SOURCES}
    ${FLIGHT_SOURCES})
target_link_libraries(gcs pthread)
target_include_directories(gcs PRIVATE src)

add_executable(autopilot
    src/autopilot_main.cpp
    ${NETWORK_SOURCES}
    ${FLIGHT_SOURCES})
target_link_libraries(autopilot pthread)
target_include_directories(autopilot PRIVATE src)
