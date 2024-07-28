cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(ParloPlusPlus VERSION 1.0)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include vcpkg toolchain file
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()

# Add the source files
set(SOURCES
    Logger.cpp
    ProcessingBuffer.cpp
    NetworkClient.cpp
    Listener.cpp
    Socket.cpp
    GoodbyePacket.cpp
    HeartbeatPacket.cpp
    # Add other source files here
)

# Add the header files
set(HEADERS
    pch.h
    framework.h
    ParloIDs.h
    EncryptionArgs.h
    EncryptionMode.h
    ProcessingBuffer.h
    EncryptedPacket.h
    Logger.h
    NetworkClient.h
    Listener.h
    Socket.h
    BlockingQueue.h
    ProcessingBuffer.h
    HeartbeatPacket.h
    Packet.h
    ParloIDs.h
    Packet.h
    GoodbyePacket.h
    HeartbeatPacket.h
    # Add other header files here
)

# Add the library
add_library(ParloPlusPlus SHARED ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(ParloPlusPlus PRIVATE ${CMAKE_SOURCE_DIR})

# Link libraries with vcpkg
find_package(asio CONFIG REQUIRED)
find_package(cryptopp CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

# Add any required libraries here
target_link_libraries(ParloPlusPlus PRIVATE asio::asio cryptopp::cryptopp ZLIB::ZLIB)

# Platform-specific libraries
if (WIN32)
    target_link_libraries(ParloPlusPlus PRIVATE ws2_32)
elseif (UNIX)
    target_link_libraries(ParloPlusPlus PRIVATE pthread)
endif()
