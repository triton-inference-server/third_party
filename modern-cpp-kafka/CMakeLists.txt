cmake_minimum_required(VERSION "3.8")

project("Modern C++ Kafka API" VERSION 1.0.0)

get_property(parent_directory DIRECTORY PROPERTY PARENT_DIRECTORY)
if (NOT parent_directory)
    set(cppkafka_master_project ON)
    # Use Strict Options
    if ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang") OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
        add_compile_options("-Wall" "-Werror" "-Wextra" "-Wshadow" "-Wno-unused-result")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    endif ()
    if (CMAKE_CXX_STANDARD EQUAL 14)
        add_compile_options("-Wno-maybe-uninitialized")
    endif ()
endif ()

include(CheckCXXCompilerFlag)
include(CMakePushCheckState)


#---------------------------
# C++17 (by default)
#---------------------------
if (NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif ()
set(CMAKE_CXX_STANDARD_REQUIRED False)

#---------------------------
# librdkafka library
#---------------------------
if (DEFINED ENV{LIBRDKAFKA_ROOT})
    set(LIBRDKAFKA_INCLUDE_DIR $ENV{LIBRDKAFKA_ROOT}/include)
    set(LIBRDKAFKA_LIBRARY_DIR $ENV{LIBRDKAFKA_ROOT}/lib)
else ()
    set(LIBRDKAFKA_INCLUDE_DIR /usr/local/include)
    set(LIBRDKAFKA_LIBRARY_DIR /usr/local/lib)
endif ()

if (EXISTS "${LIBRDKAFKA_INCLUDE_DIR}/librdkafka/rdkafka.h")
    message(STATUS "librdkafka include directory: ${LIBRDKAFKA_INCLUDE_DIR}")
else ()
    message(FATAL_ERROR "Could not find headers: librdkafka!")
endif ()

if (EXISTS "${LIBRDKAFKA_LIBRARY_DIR}/librdkafka.a" OR EXISTS "${LIBRDKAFKA_LIBRARY_DIR}/librdkafka.so" OR EXISTS "${LIBRDKAFKA_LIBRARY_DIR}/rdkafka.lib" )
    message(STATUS "librdkafka library directory: ${LIBRDKAFKA_LIBRARY_DIR}")
else ()
    message(FATAL_ERROR "Could not find library: librdkafka!")
endif ()


#---------------------------
# pthread library (for linux only)
#---------------------------
if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    find_library(PTHREAD_LIB pthread)
    if (PTHREAD_LIB)
        message(STATUS "pthread library: ${PTHREAD_LIB}")
    else ()
        message(FATAL_ERROR "Could not find library: pthread!")
    endif ()
endif ()

#---------------------------
# Build Sub-directories
#---------------------------
add_subdirectory("include")
