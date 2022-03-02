########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 3.1)
project(SoapySpyServer CXX)

find_package(SoapySDR "0.7" NO_MODULE)
if(NOT SoapySDR_FOUND)
    message(FATAL_ERROR "Soapy SDR development files not found...")
endif()

########################################################################
# Find dependencies
########################################################################
find_package(Volk REQUIRED)

########################################################################
# Build SoapySDR SpyServer support module
########################################################################
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/sdrpp)

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

        sdrpp/spyserver_client.cpp
        sdrpp/utils/networking.cpp
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
