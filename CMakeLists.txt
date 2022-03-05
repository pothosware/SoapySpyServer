########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 3.1)
project(SoapySpyServer CXX)
set(CMAKE_CXX_STANDARD 14)

########################################################################
# Find dependencies
########################################################################
find_package(SoapySDR "0.8" NO_MODULE)
if(NOT SoapySDR_FOUND)
    message(FATAL_ERROR "Soapy SDR development files not found...")
endif()
find_package(Volk REQUIRED)

########################################################################
# Build SoapySDR SpyServer support module
########################################################################
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/SDRPlusPlus
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ThreadSafeQueue)

set(libraries Volk::volk)

if(WIN32)
    list(APPEND libraries
        wsock32
        ws2_32)
endif()

SOAPY_SDR_MODULE_UTIL(
    TARGET SpyServerSupport
    SOURCES
        Registration.cpp
        Settings.cpp
        Streaming.cpp

        3rdparty/SDRPlusPlus/spyserver_client.cpp
        3rdparty/SDRPlusPlus/utils/networking.cpp
    LIBRARIES
        ${libraries}
)

########################################################################
# uninstall target
########################################################################
add_custom_target(uninstall
    "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)
