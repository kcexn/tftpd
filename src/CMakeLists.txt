set(tftplib_SOURCES
  argument_parser.cpp
  tftp_server.cpp
  filesystem.cpp
)
add_library(
  tftplib
  OBJECT
  ${tftplib_SOURCES}
)
target_include_directories(
  tftplib
  PUBLIC
  ${PROJECT_SOURCE_DIR}/include/
)
target_link_libraries(
  tftplib
  PUBLIC
  cppnet
  spdlog::spdlog_header_only
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_executable(
  tftpd
  main.cpp
)
target_link_libraries(
  tftpd
  PRIVATE
  tftplib
)
install(TARGETS tftpd)
