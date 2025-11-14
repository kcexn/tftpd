cmake_minimum_required(VERSION 3.28)
project(tftpd VERSION 0.1.0 LANGUAGES CXX)

# echo requires at least C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs)

# Get CPM
# For more information on how to add CPM to your project, see: https://github.com/cpm-cmake/CPM.cmake#adding-cpm
include(cmake/CPM.cmake)

# Add cppnet as a dependency.
CPMAddPackage("gh:kcexn/cppnet@0.7.5")

# Add spdlog as a dependency.
CPMAddPackage("gh:gabime/spdlog@1.16.0")

option(TFTP_BUILD_TESTING "Enable testing." OFF)
if (TFTP_BUILD_TESTING)
  # Add GoogleTest
  message(STATUS "Configure tests with GoogleTest")
  CPMAddPackage(
    NAME googletest
    URL "https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip"
    EXCLUDE_FROM_ALL YES
    SYSTEM YES
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

  option(TFTP_ENABLE_COVERAGE "Enable code-coverage." ON)
  if (TFTP_ENABLE_COVERAGE)
    include(cmake/EnableCoverage.cmake)
  endif()

  # Enable tests.
  include(CTest)
  add_subdirectory(tests)

  message(STATUS "GoogleTest configured successfully")
endif()

option(TFTP_BUILD_DOCS "Build documentation." OFF)
if (TFTP_BUILD_DOCS)
  include(cmake/EnableDocs.cmake)
endif()

# Add targets
add_subdirectory(src)

set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_SOURCE_IGNORE_FILES "/[^R].*\\\\.md;/\\\\..*;/.*\\\\.json;/tests/.+;/docs/;/build/")
include(CPack)
