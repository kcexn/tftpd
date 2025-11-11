# TFTP daemon

[![Tests](https://github.com/kcexn/rfc862-echo/actions/workflows/tests.yml/badge.svg)](https://github.com/kcexn/rfc862-echo/actions/workflows/tests.yml)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/34e5299bc2954055a5a466b3d0e03249)](https://app.codacy.com/gh/kcexn/tftpd/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
[![Codacy Badge](https://app.codacy.com/project/badge/Coverage/34e5299bc2954055a5a466b3d0e03249)](https://app.codacy.com/gh/kcexn/tftpd/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_coverage)

A modern TFTP (Trivial File Transfer Protocol) server written in C++20. Implements [RFC 1350](https://www.rfc-editor.org/rfc/rfc1350).

## Features

- **RFC 1350 compliant**: Full support for TFTP protocol (RRQ, WRQ, DATA, ACK, ERROR)
- **Transfer modes**: NETASCII, OCTET, and MAIL modes
- **Dual-stack networking**: IPv6 with IPv4 compatibility
- **Concurrent sessions**: Handles multiple file transfers simultaneously using async I/O
- **Adaptive timeouts**: RTT-based timeout adjustment for reliable transfers
- **Modern C++20**: Leverages coroutines, concepts, and stdexec for async operations

## Quick Start

### Building

```bash
# Debug build with tests
cmake --preset debug
cmake --build build/debug

# Release build (optimized)
cmake --preset release
cmake --build build/release
```

### Installing

Install the server to your system (default prefix: `/usr/local`):

```bash
# Build the release version first
cmake --preset release
cmake --build build/release

# Install (may require sudo)
sudo cmake --install build/release
```

This installs the `tftpd` executable to `/usr/local/bin/` by default.

#### Custom Install Location

To install to a custom location, set the `CMAKE_INSTALL_PREFIX`:

```bash
# Configure with custom prefix
cmake --preset release -DCMAKE_INSTALL_PREFIX=/opt/tftp
cmake --build build/release

# Install to custom location
sudo cmake --install build/release
```

### Uninstalling

To remove the installed files:

```bash
# From your build directory
cat build/release/install_manifest.txt | sudo xargs rm
```

Alternatively, if you no longer have the build directory:

```bash
# Manual removal (adjust paths if using custom prefix)
sudo rm /usr/local/bin/tftpd
```

### Running

```bash
# Run on default port 69 (requires root/sudo)
sudo ./build/release/bin/tftpd

# Run on custom port
./build/release/bin/tftpd -p 6969

# Set log level
./build/release/bin/tftpd -l debug -p 6969

# Set mail directory prefix
./build/release/bin/tftpd -m /var/mail -p 6969
```

### Command-line Options

- `-h, --help` - Display help message
- `-p, --port=<PORT>` - Port to listen on (default: 69)
- `-l, --log-level=<LEVEL>` - Log level: critical, error, warn, info, debug (default: info)
- `-m, --mail-prefix=<PATH>` - Mail directory prefix for MAIL mode transfers

## Testing

```bash
# Run all tests
cmake --preset debug
cmake --build build/debug
ctest --test-dir build/debug

# Generate code coverage report
cmake --build build/debug --target coverage
# View: build/debug/coverage/index.html
```

## Dependencies

- **stdexec** - NVIDIA's sender/receiver async framework
- **AsyncBerkeley** - Async Berkeley sockets interface
- **cppnet** (cloudbus-net) - Networking utilities
- **spdlog** - Fast C++ logging library

All dependencies are automatically fetched via CPM.cmake during build.

## Architecture

Built on stdexec's sender/receiver async model:

- **UDP demultiplexing**: Each client connection becomes an independent session
- **Session management**: Sessions tracked in multimap keyed by client address
- **Async file I/O**: Non-blocking file operations using stdexec senders
- **Adaptive retransmission**: Timeout adjusts based on measured round-trip time

## Protocol Support

### Read Requests (RRQ)

Clients can download files from the server. Files are read in 512-byte blocks and transmitted sequentially.

### Write Requests (WRQ)

Clients can upload files to the server. Data is written to a temporary file first, then atomically renamed on successful completion.

### Error Handling

Comprehensive error reporting with standard TFTP error codes:

- File not found (1)
- Access violation (2)
- Disk full (3)
- Illegal operation (4)
- Unknown transfer ID (5)
- File already exists (6)
- No such user (7)

## Requirements

- C++20 compiler (GCC 11+, Clang 14+)
- CMake 3.28+
- Ninja build system (recommended)

## License

GPL-3.0 - See source files for full license text.

## Contributing

This project uses:

- **clang-format** for code formatting
- **clang-tidy** for static analysis
- **GoogleTest** for unit testing
- **gcov/gcovr** for code coverage
