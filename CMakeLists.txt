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
    Packet.cpp
    # Add other source files here
)

# Add the header files
set(HEADERS
    pch.h
    framework.h
    ParloIDs.h
    EncryptionArgs.h
    EncryptionMode.h
    EncryptedPacket.h
    Logger.h
    Socket.h
    BlockingQueue.h
    HeartbeatPacket.h
    Parlo.h
    GoodbyePacket.h
    HeartbeatPacket.h
    PacketHeaders.h
    # Add other header files here
)

# Add the library
add_library(ParloPlusPlus SHARED ${SOURCES} ${HEADERS})

# Define PARLO_EXPORTS when building the library
target_compile_definitions(ParloPlusPlus PRIVATE PARLO_EXPORTS)

# Include directories
target_include_directories(ParloPlusPlus PRIVATE ${CMAKE_SOURCE_DIR})

# Link libraries with vcpkg
find_package(asio CONFIG REQUIRED)
find_package(cryptopp CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

# Add the test executable
find_package(GTest CONFIG REQUIRED)
add_executable(ProcessingBufferTests tests/ProcessingBufferTests.cpp)
target_link_libraries(ProcessingBufferTests PRIVATE ParloPlusPlus GTest::gtest GTest::gtest_main)

# Ensure the test executable can find the headers
target_include_directories(ProcessingBufferTests PRIVATE ${CMAKE_SOURCE_DIR})

# Add a test to CTest
enable_testing()
add_test(NAME ProcessingBufferTests COMMAND ProcessingBufferTests)

# Add any required libraries here
target_link_libraries(ParloPlusPlus PRIVATE asio::asio cryptopp::cryptopp ZLIB::ZLIB)

# Platform-specific libraries
if (WIN32)
    target_link_libraries(ParloPlusPlus PRIVATE ws2_32)
    target_compile_definitions(ParloPlusPlus PRIVATE _WIN32_WINNT=0x0601)
elseif (UNIX)
    target_link_libraries(ParloPlusPlus PRIVATE pthread)
endif()
