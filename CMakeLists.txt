cmake_minimum_required(VERSION 3.20)
project(PrismaFX VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(PRISMA_BUILD_TESTS "Build the ofxtest host and the test suite (downloads OpenFX HostSupport)" ON)

# ---------------------------------------------------------------- plugin bundle
# Adding an effect = adding a .cpp under src/effects; it is picked up here.
file(GLOB_RECURSE PRISMA_EFFECT_SOURCES CONFIGURE_DEPENDS src/effects/*.cpp)

add_library(PrismaFX MODULE src/core/Core.cpp src/ofx/OfxPlugin.cpp ${PRISMA_EFFECT_SOURCES})
target_include_directories(PrismaFX PRIVATE src external/openfx/include)
set_target_properties(PrismaFX PROPERTIES PREFIX "" SUFFIX ".ofx")
if(MSVC)
  target_compile_options(PrismaFX PRIVATE /W4 /fp:fast)
else()
  target_compile_options(PrismaFX PRIVATE -Wall -Wextra -O3)
endif()

# OFX bundle layout: PrismaFX.ofx.bundle/Contents/<arch>/PrismaFX.ofx
if(WIN32)
  set(PRISMA_OFX_ARCH Win64)
elseif(APPLE)
  set(PRISMA_OFX_ARCH MacOS)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
  set(PRISMA_OFX_ARCH Linux-arm-64)
else()
  set(PRISMA_OFX_ARCH Linux-x86-64)
endif()
set(PRISMA_BUNDLE_DIR "${CMAKE_BINARY_DIR}/bundle/PrismaFX.ofx.bundle")
set_target_properties(PrismaFX PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${PRISMA_BUNDLE_DIR}/Contents/${PRISMA_OFX_ARCH}"
  LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PRISMA_BUNDLE_DIR}/Contents/${PRISMA_OFX_ARCH}"
  LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PRISMA_BUNDLE_DIR}/Contents/${PRISMA_OFX_ARCH}")
if(APPLE)
  configure_file(cmake/Info.plist.in "${PRISMA_BUNDLE_DIR}/Contents/Info.plist" @ONLY)
endif()

install(DIRECTORY "${PRISMA_BUNDLE_DIR}" DESTINATION .)

# ---------------------------------------------------------------- test host
if(PRISMA_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(openfx
    GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openfx.git
    GIT_TAG c3f1c2dcf4f51762b794c4fc21181b4a501be1bf
    # Only fetch the sources; we build HostSupport ourselves below.
    SOURCE_SUBDIR do-not-add)
  FetchContent_MakeAvailable(openfx)
  find_package(EXPAT REQUIRED)

  file(GLOB OFX_HOSTSUPPORT_SOURCES ${openfx_SOURCE_DIR}/HostSupport/src/*.cpp)
  add_library(OfxHostSupport STATIC ${OFX_HOSTSUPPORT_SOURCES})
  target_include_directories(OfxHostSupport PUBLIC ${openfx_SOURCE_DIR}/include ${openfx_SOURCE_DIR}/HostSupport/include)
  target_link_libraries(OfxHostSupport PUBLIC EXPAT::EXPAT ${CMAKE_DL_LIBS})
  set_target_properties(OfxHostSupport PROPERTIES CXX_VISIBILITY_PRESET default)

  add_executable(ofxtest tools/ofxtest/ofxtest.cpp)
  target_link_libraries(ofxtest PRIVATE OfxHostSupport)
  add_dependencies(ofxtest PrismaFX)

  enable_testing()
  add_test(NAME render_all
    COMMAND ${CMAKE_COMMAND} -E env
      OFXTEST=$<TARGET_FILE:ofxtest> BUNDLE_DIR=${CMAKE_BINARY_DIR}/bundle OUT_DIR=${CMAKE_BINARY_DIR}/test-out
      bash ${CMAKE_SOURCE_DIR}/tests/run_tests.sh)
endif()
