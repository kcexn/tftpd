set(tftplib_SOURCES
  argument_parser.cpp
  tftp_server.cpp
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
  asyncberk
  cppnet
  STDEXEC::stdexec
  spdlog::spdlog_header_only
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_executable(
  tftp-server
  main.cpp
)
target_link_libraries(
  tftp-server
  PRIVATE
  tftplib
)
install(TARGETS tftp-server)
