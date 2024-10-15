# Copyright 2021-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
cmake_minimum_required(VERSION 3.17)
project(tritonthirdparty LANGUAGES C CXX)
#
# ExternalProject Dependencies
#
# For each ExternalProject dependency we use PATCH_COMMAND to copy the
# project source into TRITON_THIRD_PARTY_SRC_INSTALL_PREFIX. If
# TRITON_THIRD_PARTY_SRC_INSTALL_PREFIX is not set then source will
# not be installed. Note that PATCH_COMMAND is executed after the
# source is downloaded/updated and is not defined by default so it
# allows a good point to perform the source install.
#
include(ExternalProject)
if(NOT TRITON_THIRD_PARTY_INSTALL_PREFIX)
  message(FATAL_ERROR "TRITON_THIRD_PARTY_INSTALL_PREFIX must be set")
endif() # TRITON_THIRD_PARTY_INSTALL_PREFIX
if(NOT TRITON_THIRD_PARTY_SRC_INSTALL_PREFIX)
  message(STATUS "TRITON_THIRD_PARTY_SRC_INSTALL_PREFIX not set")
  set(INSTALL_SRC_DEST_ARG "")
else()
  set(INSTALL_SRC_DEST_ARG --dest ${TRITON_THIRD_PARTY_SRC_INSTALL_PREFIX})
endif() # TRITON_THIRD_PARTY_INSTALL_PREFIX
# If CMAKE_TOOLCHAIN_FILE is set, propagate that hint path to the external
# projects.
set(_CMAKE_ARGS_CMAKE_TOOLCHAIN_FILE "")
if (CMAKE_TOOLCHAIN_FILE)
  set(_CMAKE_ARGS_CMAKE_TOOLCHAIN_FILE "-DCMAKE_TOOLCHAIN_FILE:PATH=${CMAKE_TOOLCHAIN_FILE}")
endif()
# If VCPKG_TARGET_TRIPLET is set, propagate that hint path to the external
# projects.
set(_CMAKE_ARGS_VCPKG_TARGET_TRIPLET "")
if (VCPKG_TARGET_TRIPLET)
  set(_CMAKE_ARGS_VCPKG_TARGET_TRIPLET "-DVCPKG_TARGET_TRIPLET:STRING=${VCPKG_TARGET_TRIPLET}")
endif()
# If OPENSSL_ROOT_DIR is set, propagate that hint path to the external
# projects with OpenSSL dependency.
set(_CMAKE_ARGS_OPENSSL_ROOT_DIR "")
if (OPENSSL_ROOT_DIR)
  set(_CMAKE_ARGS_OPENSSL_ROOT_DIR "-DOPENSSL_ROOT_DIR:PATH=${OPENSSL_ROOT_DIR}")
endif()

# Some libs are installed to ${TRITON_THIRD_PARTY_INSTALL_PREFIX}/{LIB}/lib64 instead
# of ${TRITON_THIRD_PARTY_INSTALL_PREFIX}/{LIB}/lib on Centos
set (LIB_DIR "lib")
if(LINUX)
  file(STRINGS "/etc/os-release" DISTRO_ID_LIKE REGEX "ID_LIKE")
  if(${DISTRO_ID_LIKE} MATCHES "rhel|centos")
    set (LIB_DIR "lib64")
  endif(${DISTRO_ID_LIKE} MATCHES "rhel|centos")
endif(LINUX)
set(TRITON_CORE_HEADERS_ONLY OFF)

# C++ standard
set(TRITON_MIN_CXX_STANDARD 17 CACHE STRING "The minimum C++ standard whose features are requested to build this target.")

#
# Build libcurl
#
set(LIBCURL_EXTRA_BUILD_ARGS "")
if (WIN32)
  set(LIBCURL_EXTRA_BUILD_ARGS "-DCURL_USE_SCHANNEL:BOOL=ON")
endif()
ExternalProject_Add(curl
  PREFIX curl
  GIT_REPOSITORY "https://github.com/curl/curl.git"
  GIT_TAG "curl-7_86_0"
  SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/curl/src/curl"
  EXCLUDE_FROM_ALL ON
  CMAKE_CACHE_ARGS
    ${_CMAKE_ARGS_CMAKE_TOOLCHAIN_FILE}
    ${_CMAKE_ARGS_VCPKG_TARGET_TRIPLET}
    -DCMAKE_CXX_STANDARD:STRING=${TRITON_MIN_CXX_STANDARD}
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
    -DBUILD_TESTING:BOOL=OFF
    -DBUILD_CURL_EXE:BOOL=OFF
    -DBUILD_SHARED_LIBS:BOOL=OFF
    -DCURL_STATICLIB:BOOL=ON
    -DHTTP_ONLY:BOOL=ON
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/curl
    ${LIBCURL_EXTRA_BUILD_ARGS}
  PATCH_COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/install_src.py --src <SOURCE_DIR> ${INSTALL_SRC_DEST_ARG}
)
ExternalProject_Add(grpc-repo
  PREFIX grpc-repo
  GIT_REPOSITORY "https://github.com/grpc/grpc.git"
  GIT_TAG "v1.54.3"
  SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/grpc-repo/src/grpc"
  EXCLUDE_FROM_ALL ON
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  TEST_COMMAND ""
  CMAKE_CACHE_ARGS
    -DCMAKE_CXX_STANDARD:STRING=${TRITON_MIN_CXX_STANDARD}
  PATCH_COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/install_src.py --src <SOURCE_DIR> ${INSTALL_SRC_DEST_ARG} --dest-basename=grpc_1.54.3
)
#
# Build nlohmann/json
#
ExternalProject_Add(nlohmann-json
  PREFIX nlohmann-json
  GIT_REPOSITORY "https://github.com/nlohmann/json.git"
  GIT_TAG "v3.11.3"
  SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/json"
  EXCLUDE_FROM_ALL ON
  CMAKE_CACHE_ARGS
    -DJSON_MultipleHeaders:BOOL=ON
    -DCMAKE_CXX_STANDARD:STRING=${TRITON_MIN_CXX_STANDARD}
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=TRUE
    -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/nlohmann_json
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DBUILD_SHARED_LIBS:STRING=yes
    -DBUILD_TESTING:BOOL=OFF
  PATCH_COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/install_src.py --src <SOURCE_DIR> ${INSTALL_SRC_DEST_ARG} --dest-basename=nlohmann-json
)
#
# Build absl (abseil) project from grpc-repo
#
# Source is from grpc-repo so don't need PATCH_COMMAND to install
# source for this target.
#
ExternalProject_Add(absl
  PREFIX absl
  SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/grpc-repo/src/grpc/third_party/abseil-cpp"
  EXCLUDE_FROM_ALL ON
  DOWNLOAD_COMMAND ""
  CMAKE_CACHE_ARGS
    -DCMAKE_CXX_STANDARD:STRING=${TRITON_MIN_CXX_STANDARD}
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=TRUE
    -DABSL_PROPAGATE_CXX_STD:BOOL=TRUE
    -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/absl
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DBUILD_TESTING:BOOL=OFF
  DEPENDS grpc-repo
)
#
# Build protobuf project from grpc-repo
#
ExternalProject_Add(protobuf
  PREFIX protobuf
  SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/grpc-repo/src/grpc/third_party/protobuf/cmake"
  EXCLUDE_FROM_ALL ON
  DOWNLOAD_COMMAND ""
  CMAKE_CACHE_ARGS
    ${_CMAKE_ARGS_CMAKE_TOOLCHAIN_FILE}
    ${_CMAKE_ARGS_VCPKG_TARGET_TRIPLET}
    -DCMAKE_CXX_STANDARD:STRING=${TRITON_MIN_CXX_STANDARD}
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
    -Dprotobuf_BUILD_TESTS:BOOL=OFF
    -Dprotobuf_WITH_ZLIB:BOOL=OFF
    -Dprotobuf_MSVC_STATIC_RUNTIME:BOOL=OFF
    -Dprotobuf_ABSL_PROVIDER:STRING=package
    -Dabsl_DIR:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/absl/${LIB_DIR}/cmake/absl
    -DCMAKE_BUILD_TYPE:STRING=RELEASE
    -DBUILD_SHARED_LIBS:STRING=no
    -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/protobuf
  DEPENDS grpc-repo absl
)

#
# Build re2 project from grpc-repo
#
#
ExternalProject_Add(re2
    PREFIX re2
    SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/grpc-repo/src/grpc/third_party/re2"
    EXCLUDE_FROM_ALL ON
    DOWNLOAD_COMMAND ""
    CMAKE_CACHE_ARGS
    -DCMAKE_CXX_STANDARD:STRING=${TRITON_MIN_CXX_STANDARD}
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=TRUE
    -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/re2
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    DEPENDS grpc-repo
)

#
# Build googletest project from grpc-repo
#
# Source is from grpc-repo so don't need PATCH_COMMAND to install
# source for this target.
#
ExternalProject_Add(googletest
  PREFIX googletest
  SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/grpc-repo/src/grpc/third_party/googletest"
  EXCLUDE_FROM_ALL ON
  DOWNLOAD_COMMAND ""
  CMAKE_CACHE_ARGS
    ${_CMAKE_ARGS_CMAKE_TOOLCHAIN_FILE}
    ${_CMAKE_ARGS_VCPKG_TARGET_TRIPLET}
    -DCMAKE_CXX_STANDARD:STRING=${TRITON_MIN_CXX_STANDARD}
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/googletest
  DEPENDS grpc-repo
)
# Location where protobuf-config.cmake will be installed varies by
# platform
if (WIN32)
  set(_FINDPACKAGE_PROTOBUF_CONFIG_DIR "${TRITON_THIRD_PARTY_INSTALL_PREFIX}/protobuf/cmake")
else()
  set(_FINDPACKAGE_PROTOBUF_CONFIG_DIR "${TRITON_THIRD_PARTY_INSTALL_PREFIX}/protobuf/${LIB_DIR}/cmake/protobuf")
endif()
#
# Build c-ares project from grpc-repo
#
# Source is from grpc-repo so don't need PATCH_COMMAND to install
# source for this target.
#
ExternalProject_Add(c-ares
  PREFIX c-ares
  SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/grpc-repo/src/grpc/third_party/cares/cares"
  EXCLUDE_FROM_ALL ON
  DOWNLOAD_COMMAND ""
  CMAKE_CACHE_ARGS
    ${_CMAKE_ARGS_CMAKE_TOOLCHAIN_FILE}
    ${_CMAKE_ARGS_VCPKG_TARGET_TRIPLET}
    -DCMAKE_CXX_STANDARD:STRING=${TRITON_MIN_CXX_STANDARD}
    -DCARES_SHARED:BOOL=OFF
    -DCARES_STATIC:BOOL=ON
    -DCARES_STATIC_PIC:BOOL=ON
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/c-ares
  DEPENDS grpc-repo
)
#
# Build GRPC
#
# Source is from grpc-repo so don't need PATCH_COMMAND to install
# source for this target.
#
ExternalProject_Add(grpc
    PREFIX grpc
    SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/grpc-repo/src/grpc"
    EXCLUDE_FROM_ALL ON
    DOWNLOAD_COMMAND ""
    CMAKE_CACHE_ARGS
    -DCMAKE_CXX_STANDARD:STRING=${TRITON_MIN_CXX_STANDARD}
    -DgRPC_INSTALL:BOOL=ON
    -DgRPC_BUILD_TESTS:BOOL=OFF
    -DgRPC_PROTOBUF_PROVIDER:STRING=package
    -DgRPC_PROTOBUF_PACKAGE_TYPE:STRING=CONFIG
    -DgRPC_ZLIB_PROVIDER:STRING=package
    -DgRPC_CARES_PROVIDER:STRING=package
    -DgRPC_BUILD_MSVC_MP_COUNT:STRING=-1
    -DgRPC_SSL_PROVIDER:STRING=package
    -DgRPC_RE2_PROVIDER:STRING=package
    -DgRPC_ABSL_PROVIDER:STRING=package
    -Dabsl_DIR:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/absl/${LIB_DIR}/cmake/absl
    -Dc-ares_DIR:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/c-ares/${LIB_DIR}/cmake/c-ares
    -Dre2_DIR:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/re2/${LIB_DIR}/cmake/re2
    -DProtobuf_DIR:PATH=${_FINDPACKAGE_PROTOBUF_CONFIG_DIR}
    ${_CMAKE_ARGS_OPENSSL_ROOT_DIR}
    ${_CMAKE_ARGS_CMAKE_TOOLCHAIN_FILE}
    ${_CMAKE_ARGS_VCPKG_TARGET_TRIPLET}
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/grpc
    DEPENDS grpc-repo c-ares protobuf absl re2
)

#
# Build libevent
#
ExternalProject_Add(libevent
  PREFIX libevent
  GIT_REPOSITORY "https://github.com/libevent/libevent.git"
  GIT_TAG "release-2.1.10-stable"
  SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/libevent/src/libevent"
  EXCLUDE_FROM_ALL ON
  CMAKE_CACHE_ARGS
    ${_CMAKE_ARGS_CMAKE_TOOLCHAIN_FILE}
    ${_CMAKE_ARGS_VCPKG_TARGET_TRIPLET}
    -DCMAKE_CXX_STANDARD:STRING=${TRITON_MIN_CXX_STANDARD}
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/libevent
  PATCH_COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/install_src.py --src <SOURCE_DIR> ${INSTALL_SRC_DEST_ARG}
)
#
# Build patched libevhtp
#
if (WIN32)
# FIXME evhtp can't be built successfully in native Windows environment
# CMAKE_PREFIX_PATH is needed to find libevent
ExternalProject_Add(libevhtp
  PREFIX libevhtp
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libevhtp/libevhtp"
  EXCLUDE_FROM_ALL ON
  DOWNLOAD_COMMAND ""
  CMAKE_CACHE_ARGS
    ${_CMAKE_ARGS_CMAKE_TOOLCHAIN_FILE}
    ${_CMAKE_ARGS_VCPKG_TARGET_TRIPLET}
    -DCMAKE_CXX_STANDARD:STRING=${TRITON_MIN_CXX_STANDARD}
    -DEVHTP_DISABLE_REGEX:BOOL=ON
    -DEVHTP_DISABLE_SSL:BOOL=ON
    -DEVHTP_TRITON_ENABLE_HTTP_CONTIGUOUS:BOOL=ON
    -DEVHTP_TRITON_ENABLE_TRACING:BOOL=${TRITON_ENABLE_TRACING}
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
    -DCMAKE_PREFIX_PATH:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/libevent
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/libevhtp
  PATCH_COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/install_src.py --src <SOURCE_DIR> ${INSTALL_SRC_DEST_ARG}
  DEPENDS libevent
)
else()
ExternalProject_Add(libevhtp
  PREFIX libevhtp
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libevhtp/libevhtp"
  EXCLUDE_FROM_ALL ON
  DOWNLOAD_COMMAND ""
  CMAKE_CACHE_ARGS
    ${_CMAKE_ARGS_CMAKE_TOOLCHAIN_FILE}
    ${_CMAKE_ARGS_VCPKG_TARGET_TRIPLET}
    -DCMAKE_CXX_STANDARD:STRING=${TRITON_MIN_CXX_STANDARD}
    -DEVHTP_DISABLE_REGEX:BOOL=ON
    -DEVHTP_DISABLE_SSL:BOOL=ON
    -DEVHTP_TRITON_ENABLE_HTTP_CONTIGUOUS:BOOL=ON
    -DEVHTP_TRITON_ENABLE_TRACING:BOOL=${TRITON_ENABLE_TRACING}
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
    -DLibevent_DIR:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/libevent/lib/cmake/libevent
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/libevhtp
  PATCH_COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/install_src.py --src <SOURCE_DIR> ${INSTALL_SRC_DEST_ARG}
  DEPENDS libevent
)
endif()
#
# Build Prometheus C++ library
#
ExternalProject_Add(prometheus-cpp
  PREFIX prometheus-cpp
  GIT_REPOSITORY "https://github.com/jupp0r/prometheus-cpp.git"
  GIT_TAG "v1.0.1"
  SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/prometheus-cpp/src/prometheus-cpp"
  EXCLUDE_FROM_ALL ON
  CMAKE_CACHE_ARGS
    ${_CMAKE_ARGS_CMAKE_TOOLCHAIN_FILE}
    ${_CMAKE_ARGS_VCPKG_TARGET_TRIPLET}
    -DCMAKE_CXX_STANDARD:STRING=${TRITON_MIN_CXX_STANDARD}
    -DENABLE_PUSH:BOOL=OFF
    -DENABLE_PULL:BOOL=OFF
    -DENABLE_TESTING:BOOL=OFF
    -DUSE_THIRDPARTY_LIBRARIES:BOOL=OFF
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/prometheus-cpp
  PATCH_COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/install_src.py --src <SOURCE_DIR> ${INSTALL_SRC_DEST_ARG}
)
#
# Build crc32c
#
# Use a specific hash to get ARM64 fix not included in latest release
# (https://github.com/google/crc32c/pull/52).
#
ExternalProject_Add(crc32c
  PREFIX crc32c
  GIT_REPOSITORY "https://github.com/google/crc32c.git"
  GIT_TAG "b9d6e825a1e6783195a6051639179152dac70b3b"
  SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/crc32c/src/crc32c"
  EXCLUDE_FROM_ALL ON
  CMAKE_CACHE_ARGS
    ${_CMAKE_ARGS_CMAKE_TOOLCHAIN_FILE}
    ${_CMAKE_ARGS_VCPKG_TARGET_TRIPLET}
    -DCMAKE_CXX_STANDARD:STRING=${TRITON_MIN_CXX_STANDARD}
    -DCRC32C_BUILD_TESTS:BOOL=OFF
    -DBUILD_SHARED_LIBS:STRING=no
    -DCRC32C_BUILD_BENCHMARKS:BOOL=OFF
    -DCRC32C_USE_GLOG:BOOL=OFF
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/crc32c
  PATCH_COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/install_src.py --src <SOURCE_DIR> ${INSTALL_SRC_DEST_ARG}
)
# This is necessary because google-cloud-cpp
# does not pass protobuf_DIR and gRPC_DIR etc to its
# external projects but expects them in CMAKE_PREFIX_PATH.
#
# absl must preceed grpc to ensure that the absl from the newer grpc
# repo is used and not the absl from the older grpc.
set(GCS_CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH}
   ${TRITON_THIRD_PARTY_INSTALL_PREFIX}/c-ares/${LIB_DIR}/cmake/c-ares
   ${TRITON_THIRD_PARTY_INSTALL_PREFIX}/absl/${LIB_DIR}/cmake/absl
   ${TRITON_THIRD_PARTY_INSTALL_PREFIX}/crc32c/${LIB_DIR}/cmake/Crc32c
   ${TRITON_THIRD_PARTY_INSTALL_PREFIX}/grpc/lib/cmake/grpc
   ${TRITON_THIRD_PARTY_INSTALL_PREFIX}/nlohmann_json/${LIB_DIR}/cmake/nlohmann_json
   ${_FINDPACKAGE_PROTOBUF_CONFIG_DIR})
#
# Build google-cloud-cpp
#
ExternalProject_Add(google-cloud-cpp
  PREFIX google-cloud-cpp
  GIT_REPOSITORY "https://github.com/googleapis/google-cloud-cpp.git"
  GIT_TAG "v1.42.0"
  SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/google-cloud-cpp/src/google-cloud-cpp"
  EXCLUDE_FROM_ALL ON
  CMAKE_CACHE_ARGS
    ${_CMAKE_ARGS_CMAKE_TOOLCHAIN_FILE}
    ${_CMAKE_ARGS_VCPKG_TARGET_TRIPLET}
    -DCMAKE_CXX_STANDARD:STRING=${TRITON_MIN_CXX_STANDARD}
    -DGOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER:STRING=package
    -DBUILD_TESTING:BOOL=OFF
    -DCMAKE_PREFIX_PATH:PATH=${GCS_CMAKE_PREFIX_PATH}
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
    -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/google-cloud-cpp
    -Dnlohmann_json_DIR:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/nlohmann_json/${LIB_DIR}/cmake/nlohmann_json
    -DProtobuf_DIR:PATH=${_FINDPACKAGE_PROTOBUF_CONFIG_DIR}
    -DCrc32c_DIR:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/crc32c/${LIB_DIR}/cmake/Crc32c
  PATCH_COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/install_src.py --src <SOURCE_DIR> ${INSTALL_SRC_DEST_ARG}
  DEPENDS grpc c-ares protobuf crc32c absl nlohmann-json
)
#
# Build azure-sdk (need azure-storage-blobs)
# [FIXME] azure-sdk sets CMake Standard to 14, without `CACHE`:
# https://github.com/Azure/azure-sdk-for-cpp/blob/2850c5d32c8a86491b49e801433b8f186fa81745/CMakeLists.txt#L27
# This `set` does not allow us to override `CMAKE_CXX_STANDARD`, 
# thus I am not passing flag here.


ExternalProject_Add(azure-iot-sdk-c
  PREFIX azure-iot-sdk-c
  GIT_REPOSITORY "https://github.com/Azure/azure-iot-sdk-c.git"
  GIT_TAG "LTS_03_2024_Ref02"
  SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/azure-iot-sdk-c/src/azure-iot-sdk-c"
  EXCLUDE_FROM_ALL ON
  CMAKE_CACHE_ARGS
    ${_CMAKE_ARGS_CMAKE_TOOLCHAIN_FILE}
    ${_CMAKE_ARGS_VCPKG_TARGET_TRIPLET}
    -DCMAKE_CXX_STANDARD:STRING=${TRITON_MIN_CXX_STANDARD}
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -Duse_default_uuid:bool=ON
    -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/azure-iot-sdk-c
  PATCH_COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/install_src.py --src <SOURCE_DIR> ${INSTALL_SRC_DEST_ARG} 
  DEPENDS curl
)

ExternalProject_Add(azure-sdk
  PREFIX azure-sdk
  GIT_REPOSITORY "https://github.com/Azure/azure-sdk-for-cpp.git"
  GIT_TAG "azure-storage-blobs_12.12.0"
  SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/azure-sdk/src/azure-sdk-for-cpp"
  EXCLUDE_FROM_ALL ON
  CMAKE_CACHE_ARGS
    ${_CMAKE_ARGS_CMAKE_TOOLCHAIN_FILE}
    ${_CMAKE_ARGS_VCPKG_TARGET_TRIPLET}
    -DBUILD_SHARED_LIBS:BOOL=OFF
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCURL_INCLUDE_DIR:STRING=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/curl/include
    -DCURL_LIBRARY:STRING=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/curl/${LIB_DIR}
    -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/azure-sdk
    -DDISABLE_AZURE_CORE_OPENTELEMETRY:BOOL=ON
    -DENV{AZURE_SDK_DISABLE_AUTO_VCPKG}:BOOL=ON
    -DCMAKE_PREFIX_PATH:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/azure-iot-sdk-c
  PATCH_COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/install_src.py --src <SOURCE_DIR> ${INSTALL_SRC_DEST_ARG}
  DEPENDS curl azure-iot-sdk-c
)

#
# Build CNMeM (CUDA memory management library)
#
ExternalProject_Add(cnmem
  PREFIX cnmem
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cnmem/cnmem"
  EXCLUDE_FROM_ALL ON
  DOWNLOAD_COMMAND ""
  CMAKE_CACHE_ARGS
    ${_CMAKE_ARGS_CMAKE_TOOLCHAIN_FILE}
    ${_CMAKE_ARGS_VCPKG_TARGET_TRIPLET}
    -DCMAKE_CXX_STANDARD:STRING=${TRITON_MIN_CXX_STANDARD}
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
    -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/cnmem
  PATCH_COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/install_src.py --src <SOURCE_DIR> ${INSTALL_SRC_DEST_ARG}
)
#
# Build aws-sdk-cpp
# FIXME] aws-sdk-cpp sets CMake Standard to 11, without `CACHE`:
# https://github.com/aws/aws-sdk-cpp/blob/6e9ef07d76cb9403a1b8626efa2eeff8a82c53e8/CMakeLists.txt#L353
# This `set` does not allow us to override `CMAKE_CXX_STANDARD`, 
# thus I am not passing flag here.
ExternalProject_Add(aws-sdk-cpp
  PREFIX aws-sdk-cpp
  GIT_REPOSITORY "https://github.com/aws/aws-sdk-cpp.git"
  GIT_TAG "1.11.60"
  SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/aws-sdk-cpp/src/aws-sdk-cpp"
  EXCLUDE_FROM_ALL ON
  CMAKE_CACHE_ARGS
    ${_CMAKE_ARGS_CMAKE_TOOLCHAIN_FILE}
    ${_CMAKE_ARGS_VCPKG_TARGET_TRIPLET}
    -DBUILD_ONLY:STRING=s3
    -DBUILD_SHARED_LIBS:BOOL=OFF
    -DMINIMIZE_SIZE:BOOL=ON
    -DENABLE_TESTING:BOOL=OFF
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
    -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/aws-sdk-cpp
  PATCH_COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/install_src.py --src <SOURCE_DIR> ${INSTALL_SRC_DEST_ARG}
)
#
# Build OpenTelemetry C++ library
# [FIXME] Remove `NOT WIN32` condition when Windows build works on Triton (JIRA DLIS-4786)
# OpenTelemetry requires a location of protobuf to be passed in cmake.
# PROTO_LIB_PATH is set for where protobuf library ('libprotobuf.a') is located on Ubuntu.
# Windows may have another extention for libprotobuf and different location of installation.
# `PROTO_LIB_PATH` should be properly set for Windows
#
if (NOT WIN32)
  set(PROTO_LIB_PATH "${TRITON_THIRD_PARTY_INSTALL_PREFIX}/protobuf/${LIB_DIR}/libprotobuf.a")
  ExternalProject_Add(opentelemetry-cpp
    PREFIX opentelemetry-cpp
    GIT_REPOSITORY "https://github.com/open-telemetry/opentelemetry-cpp.git"
    GIT_TAG "v1.13.0"
    SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/opentelemetry-cpp/src/opentelemetry-cpp"
    EXCLUDE_FROM_ALL ON
    CMAKE_CACHE_ARGS
      -DCMAKE_CXX_STANDARD:STRING=${TRITON_MIN_CXX_STANDARD}
      -DProtobuf_LIBRARIES:STRING=${PROTO_LIB_PATH}
      -DProtobuf_INCLUDE_DIR:STRING=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/protobuf/include
      -DProtobuf_PROTOC_EXECUTABLE:STRING=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/protobuf/bin/protoc
      -Dnlohmann_json_DIR:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/nlohmann_json/${LIB_DIR}/cmake/nlohmann_json
      ${_CMAKE_ARGS_CMAKE_TOOLCHAIN_FILE}
      ${_CMAKE_ARGS_VCPKG_TARGET_TRIPLET}
      -DBUILD_SHARED_LIBS:STRING=OFF
      -DBUILD_TESTING:BOOL=OFF
      -DWITH_EXAMPLES:BOOL=OFF 
      -DWITH_BENCHMARK:BOOL=OFF
      -DWITH_ABSEIL:BOOL=OFF
      -DWITH_OTLP_GRPC:BOOL=OFF
      -DWITH_OTLP_HTTP:BOOL=ON
      -DOPENTELEMETRY_INSTALL:BOOL=ON
      -DCURL_DIR:STRING=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/curl/${LIB_DIR}/cmake/CURL
      -DgRPC_DIR:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/grpc/lib/cmake/grpc
      -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
      -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
      -DCMAKE_INSTALL_PREFIX:PATH=${TRITON_THIRD_PARTY_INSTALL_PREFIX}/opentelemetry-cpp
    PATCH_COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/install_src.py --src <SOURCE_DIR> ${INSTALL_SRC_DEST_ARG}
    DEPENDS grpc nlohmann-json curl protobuf
  )
endif()
