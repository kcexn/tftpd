# rfc862-echo-server

[![Tests](https://github.com/kcexn/rfc862-echo/actions/workflows/tests.yml/badge.svg)](https://github.com/kcexn/rfc862-echo/actions/workflows/tests.yml)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/575775ae38044852bbc6eeefae1c83d5)](https://app.codacy.com/gh/kcexn/rfc862-echo/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
[![Codacy Badge](https://app.codacy.com/project/badge/Coverage/575775ae38044852bbc6eeefae1c83d5)](https://app.codacy.com/gh/kcexn/rfc862-echo/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_coverage)

A simple echo server. Technically implements [RFC 862](https://datatracker.ietf.org/doc/html/rfc862).

## Features

- **Supports both UDP and TCP echoing**: Listens on port 7 by default.
- **IPv4/IPv6 Dual-Stack**: Supports both IPv4 and IPv6 connections.

## Requirements

- **C++20 Compiler**: GCC 11+, Clang 14+, or MSVC 2022+
- **CMake**: 3.28 or higher
- **Ninja**: Build system (recommended)
- **gcovr**: For code coverage reports (optional, debug builds only)

## Dependencies

All dependencies are automatically fetched via CMake:

- [**stdexec**](https://github.com/NVIDIA/stdexec) - NVIDIA's C++ sender/receiver async framework
- [**async-berkeley**](https://github.com/kcexn/async-berkeley) - Async Berkeley sockets wrapper
- [**cppnet**](https://github.com/kcexn/cloudbus-net) - Networking utilities and service base classes
- [**spdlog**](https://github.com/gabime/spdlog) - Fast C++ logging library
- [**GoogleTest**](https://github.com/google/googletest) - Test suites (Optional)

## Quick Start

### Building

```bash
# Debug build with tests and coverage
cmake --preset debug
cmake --build build/debug

# Release build (optimized)
cmake --preset release
cmake --build build/release
```

### Installation

```bash
# Install (default: /usr/local/bin)
sudo cmake --install build/release

# Or specify custom prefix
cmake --preset release -DCMAKE_INSTALL_PREFIX=/opt/echo
sudo cmake --install build/release
```

### Running the Server

```bash
# Run on default port 7 (requires root/admin privileges)
sudo ./build/release/bin/echo-server

# Run on custom port with debug logging
./build/release/bin/echo-server --log-level debug 8080
```

### Testing the Server

```bash
# Using netcat
echo "Hello, World!" | nc localhost 8080

# Using telnet
telnet localhost 8080
```

## Usage

```text
echo-server [--log-level <LEVEL>] [<PORT>]

Options:
  --log-level <LEVEL>   Set logging level (trace, debug, info, warn, error, critical, off)
  -h, --help           Show help message
  <PORT>               Port number to listen on (default: 7)
```

## Development

### Running Tests

```bash
# Run all tests
cmake --preset debug
cmake --build build/debug
ctest --test-dir build/debug

# Run specific test
./build/debug/tests/test_echo_service
```

### Code Coverage

```bash
# Generate HTML coverage report
cmake --build build/debug --target coverage
# Open build/debug/coverage/index.html in browser

# Generate XML coverage for CI
cmake --build build/debug --target coverage-xml
```

### Code Style

The project uses clang-format and clang-tidy for code formatting and linting:

```bash
# Format code
find src include tests -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i

# Run linter
clang-tidy src/*.cpp -- -std=c++20 -I include/
```
