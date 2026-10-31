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

# ---------------------------------------------------------------- After Effects plugin
# Needs Adobe's After Effects SDK, which cannot be redistributed: download it
# and point PRISMA_AE_SDK at its "Examples" folder (the one with Headers,
# Resources and Util). Builds PrismaFX.aex (Windows) or PrismaFX.plugin (macOS).
set(PRISMA_AE_SDK "" CACHE PATH "After Effects SDK Examples folder; builds the AE plugin when set")
if(PRISMA_AE_SDK)
  set(AE_HEADERS "${PRISMA_AE_SDK}/Headers")
  set(AE_RESOURCES "${PRISMA_AE_SDK}/Resources")
  if(NOT EXISTS "${AE_HEADERS}/AE_Effect.h")
    message(FATAL_ERROR "PRISMA_AE_SDK=${PRISMA_AE_SDK} has no Headers/AE_Effect.h")
  endif()
  set(AE_GEN "${CMAKE_BINARY_DIR}/ae")

  # Writes the PiPL source and the exported entry points from the Registry.
  add_executable(aepipl tools/aepipl/aepipl.cpp src/core/Core.cpp ${PRISMA_EFFECT_SOURCES})
  target_include_directories(aepipl PRIVATE src)
  add_custom_command(OUTPUT "${AE_GEN}/PrismaFX_PiPL.r" "${AE_GEN}/AeEntries.cpp"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${AE_GEN}"
    COMMAND aepipl "${AE_GEN}/PrismaFX_PiPL.r" "${AE_GEN}/AeEntries.cpp"
    DEPENDS aepipl
    COMMENT "Generating the After Effects PiPL")

  add_library(PrismaFX_AE MODULE src/ae/AePlugin.cpp src/ae/AeBridge.cpp src/core/Core.cpp
    ${PRISMA_EFFECT_SOURCES} "${AE_GEN}/AeEntries.cpp")
  target_include_directories(PrismaFX_AE PRIVATE src "${AE_HEADERS}" "${AE_HEADERS}/SP" "${AE_HEADERS}/adobesdk"
    "${PRISMA_AE_SDK}/Util")
  set_target_properties(PrismaFX_AE PROPERTIES OUTPUT_NAME PrismaFX PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/ae-plugin"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/ae-plugin"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/ae-plugin")
  if(WIN32)
    target_compile_definitions(PrismaFX_AE PRIVATE MSWindows WIN32 _WINDOWS NOMINMAX)
    target_compile_options(PrismaFX_AE PRIVATE /fp:fast)
    set_target_properties(PrismaFX_AE PROPERTIES SUFFIX ".aex")
    add_custom_command(OUTPUT "${AE_GEN}/PrismaFX_PiPL.rc"
      COMMAND ${CMAKE_COMMAND} "-DCL=${CMAKE_CXX_COMPILER}" "-DPIPLTOOL=${AE_RESOURCES}/PiPLtool.exe"
        "-DHEADERS=${AE_HEADERS}" "-DIN=${AE_GEN}/PrismaFX_PiPL.r" "-DOUT=${AE_GEN}/PrismaFX_PiPL.rc"
        -P "${CMAKE_SOURCE_DIR}/cmake/AePiPL.cmake"
      DEPENDS "${AE_GEN}/PrismaFX_PiPL.r" "${CMAKE_SOURCE_DIR}/cmake/AePiPL.cmake"
      COMMENT "Compiling the PiPL resource")
    target_sources(PrismaFX_AE PRIVATE "${AE_GEN}/PrismaFX_PiPL.rc")
  elseif(APPLE)
    configure_file(cmake/AeInfo.plist.in "${AE_GEN}/Info.plist" @ONLY)
    target_compile_options(PrismaFX_AE PRIVATE -O3)
    set_target_properties(PrismaFX_AE PROPERTIES BUNDLE TRUE BUNDLE_EXTENSION plugin
      MACOSX_BUNDLE_INFO_PLIST "${AE_GEN}/Info.plist")
    add_custom_command(TARGET PrismaFX_AE POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:PrismaFX_AE>/Resources"
      COMMAND xcrun Rez -o "$<TARGET_BUNDLE_CONTENT_DIR:PrismaFX_AE>/Resources/PrismaFX.rsrc" -useDF
        -d AE_OS_MAC -d __MACH__ -i "${AE_HEADERS}" -i "${AE_RESOURCES}" -arch arm64 -arch x86_64
        "${AE_GEN}/PrismaFX_PiPL.r"
      COMMENT "Compiling the PiPL resource")
  endif()
endif()

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

  # The SDK-free half of the After Effects bridge, checked against direct renders.
  add_executable(ae_bridge_test tests/ae_bridge_test.cpp src/ae/AeBridge.cpp src/core/Core.cpp ${PRISMA_EFFECT_SOURCES})
  target_include_directories(ae_bridge_test PRIVATE src)
  add_test(NAME ae_bridge COMMAND ae_bridge_test)
  add_test(NAME render_all
    COMMAND ${CMAKE_COMMAND} -E env
      OFXTEST=$<TARGET_FILE:ofxtest> BUNDLE_DIR=${CMAKE_BINARY_DIR}/bundle OUT_DIR=${CMAKE_BINARY_DIR}/test-out
      bash ${CMAKE_SOURCE_DIR}/tests/run_tests.sh)
endif()
