cmake_minimum_required(VERSION 3.10)

#Set the project name and version
project(ParloPlusPlus VERSION 1.0)

#Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#Add the source files
set(SOURCES
    NetworkClient.cpp
    Listener.cpp
    Socket.cpp
    # Add other source files here
)

#Add the header files
set(HEADERS
    NetworkClient.h
    Listener.h
    Socket.h
    BlockingQueue.h
    ProcessingBuffer.h
    HeartbeatPacket.h
    Packet.h
    ParloIDs.h
    # Add other header files here
)

#Add the library
add_library(ParloPlusPlus SHARED ${SOURCES} ${HEADERS})

#Include directories
target_include_directories(ParloPlusPlus PUBLIC .)

#Add any required libraries here
# target_link_libraries(ParloPlusPlus <library>)
