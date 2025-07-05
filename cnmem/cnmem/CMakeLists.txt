# Copyright 2015-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# CMakeLists to build the cnmem library.
cmake_minimum_required(VERSION 2.8.8)
project(cnmem)

# We need CUDA to build that library.
find_package(CUDAToolkit QUIET REQUIRED)
message(STATUS "CUDAToolkit_VERSION: ${CUDAToolkit_VERSION}")
include_directories(${CUDAToolkit_INCLUDE_DIRS})

# Rules to build the cnmem library.
include_directories(include)
add_definitions(-DCNMEM_DLLEXPORT)
add_library(cnmem STATIC src/cnmem.cpp)
set_target_properties(cnmem PROPERTIES VERSION 1.0.0 SOVERSION 1)
target_link_libraries(cnmem LINK_PUBLIC ${CUDAToolkit_LIBRARY_DIR})
install(TARGETS cnmem RUNTIME DESTINATION bin ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
install(FILES include/cnmem.h DESTINATION include)

# Add the tests.
if(WITH_TESTS)

  # Get Google tests.
  find_package(GTest QUIET REQUIRED)
  include_directories(${GTEST_INCLUDE_DIRS})

  # Build the executable.
  add_executable(cnmem_tests tests/cnmem_tests.cpp)
  if(MSVC)
    if(MSVC_VERSION GREATER 1700) # Visual Studio 11 or more.
      add_definitions(-DUSE_CPP_11)
    endif(MSVC_VERSION GREATER 1700)
  endif(MSVC)
  if(CMAKE_COMPILER_IS_GNUCC)
    add_definitions(-std=c++11 -DUSE_CPP_11)
  endif(CMAKE_COMPILER_IS_GNUCC)
  target_link_libraries(cnmem_tests LINK_PUBLIC cnmem ${CUDAToolkit_LIBRARY_DIR} ${GTEST_LIBRARIES} -lpthread)
  install(TARGETS cnmem_tests RUNTIME DESTINATION bin)

  # Tests that launch kernels to force reading and writing to memory
  cuda_add_executable(cnmem_kernel_tests tests/cnmem_kernel_test.cu)
  target_link_libraries(cnmem_kernel_tests cnmem ${CUDAToolkit_LIBRARY_DIR} ${GTEST_LIBRARIES} -lpthread)
  install(TARGETS cnmem_kernel_tests RUNTIME DESTINATION bin)

  # On Windows, we copy the Google test DLL to the bin folder.
  if(MSVC)
    get_filename_component(gtest_dll_path ${GTEST_LIBRARIES} DIRECTORY)
    install(FILES ${gtest_dll_path}/gtest.dll DESTINATION bin)
  endif(MSVC)

endif(WITH_TESTS)

