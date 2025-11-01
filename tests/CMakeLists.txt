include(GoogleTest)

set(TEST_NAMES
  test_argument_parser
  test_generator
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
