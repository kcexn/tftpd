include(GoogleTest)

set(TEST_NAMES
  test_argument_parser
  test_endian
  test_filesystem
  test_generator
  test_tftp_protocol
  test_tftp_server_static
  test_tftp_server
)

foreach(TEST_NAME IN LISTS TEST_NAMES)
  add_executable(
    ${TEST_NAME}
    ${TEST_NAME}.cpp
  )

  target_include_directories(${TEST_NAME}
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
  )

  target_link_libraries(
    ${TEST_NAME}
    PRIVATE
    tftplib
    GTest::gtest_main
  )

  if(TFTP_ENABLE_COVERAGE)
    target_link_libraries(${TEST_NAME} PRIVATE gcov)
  endif()

  gtest_discover_tests(${TEST_NAME})
endforeach()

# Helper function to add environment-based tests
# Usage: add_env_test(<test_name> <test_suffix> <gtest_filter> [ENV_VAR=value ...])
function(add_env_test TEST_NAME TEST_SUFFIX GTEST_FILTER)
  add_test(
    NAME ${TEST_NAME}.${TEST_SUFFIX}
    COMMAND ${TEST_NAME} --gtest_filter=${GTEST_FILTER}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )

  # If environment variables are provided, set them
  if(ARGC GREATER 3)
    set_tests_properties(${TEST_NAME}.${TEST_SUFFIX}
      PROPERTIES
        ENVIRONMENT "${ARGN}"
    )
  endif()
endfunction()

# Define environment-based tests
# Format: test_name|test_suffix|gtest_filter|env_var1|env_var2|...
set(ENV_TESTS
  "test_filesystem_env|default|*.ReturnsDefaultPathWhenEnvNotSet"
  "test_filesystem_env|custom|*.ReturnsCustomPathWhenEnvSet|TFTP_MAIL_PREFIX=/custom/test/path"
)

# Build and register environment-based tests
foreach(ENV_TEST_SPEC IN LISTS ENV_TESTS)
  # Parse the test specification
  string(REPLACE "|" ";" ENV_TEST_SPEC_LIST "${ENV_TEST_SPEC}")
  list(GET ENV_TEST_SPEC_LIST 0 TEST_NAME)
  list(GET ENV_TEST_SPEC_LIST 1 TEST_SUFFIX)
  list(GET ENV_TEST_SPEC_LIST 2 GTEST_FILTER)

  # Get environment variables (if any)
  list(LENGTH ENV_TEST_SPEC_LIST SPEC_LENGTH)
  if(SPEC_LENGTH GREATER 3)
    list(SUBLIST ENV_TEST_SPEC_LIST 3 -1 ENV_VARS)
  else()
    set(ENV_VARS "")
  endif()

  # Build the test executable only once per test name
  if(NOT TARGET ${TEST_NAME})
    add_executable(
      ${TEST_NAME}
      ${TEST_NAME}.cpp
    )

    target_include_directories(${TEST_NAME}
      PRIVATE
      ${PROJECT_SOURCE_DIR}/include
    )

    target_link_libraries(
      ${TEST_NAME}
      PRIVATE
      tftplib
      GTest::gtest_main
    )

    if(TFTP_ENABLE_COVERAGE)
      target_link_libraries(${TEST_NAME} PRIVATE gcov)
    endif()
  endif()

  # Register the test with environment variables
  if(ENV_VARS)
    add_env_test(${TEST_NAME} ${TEST_SUFFIX} ${GTEST_FILTER} ${ENV_VARS})
  else()
    add_env_test(${TEST_NAME} ${TEST_SUFFIX} ${GTEST_FILTER})
  endif()
endforeach()
