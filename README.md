# Repository Deprecated

This repository is deprecated and no longer maintained. Its contents have been merged into the [WujihandPy](https://github.com/Wuji-Technology-Co-Ltd/wujihandpy) repository as a subtree. Please use and contribute to WujihandPy for all development, issues, and pull requests:

- [https://github.com/Wuji-Technology-Co-Ltd/wujihandpy](https://github.com/Wuji-Technology-Co-Ltd/wujihandpy)

This repository is retained as read-only for historical reference.

# WujihandCpp: A Lightweight C++ SDK for Wujihand

This is a C++ SDK for the Wujihand dexterous hand, newly implemented from scratch.

It provides simpler, more efficient, and more user‑friendly interfaces to interact with the dexterous hand device.

The Python version, [WujihandPy](https://github.com/Wuji-Technology-Co-Ltd/wujihandpy) (based on pybind11 bindings), is published on PyPI and can be installed via pip.

## Minimum System Requirements (Linux)

The minimum requirements apply only when using the SDK release packages locally. The release headers depend only on C++11, and the binaries are built against an older glibc to ensure compatibility with older distributions and compilers.

If you develop with Docker, you can use any Linux distribution without concern for the system version.

If building from source, you must use a compiler with full C++20 support (GCC 13+ / Clang 17+).

### glibc 2.28+

Use a Linux distribution with glibc 2.28 or newer:
- Debian 10+
- Ubuntu 18.10+
- Fedora 29+
- CentOS/RHEL 8+

### C++11 Toolchain

Use a C++ compiler that supports C++11 or later:
- GCC 5+
- Clang 4+
<!-- - MSVC 2015+ -->

### libusb 1.0

Install the libusb 1.0 development package:

- Debian/Ubuntu: `sudo apt install libusb-1.0-0-dev`
- Fedora/CentOS/RHEL: `sudo dnf install libusbx-devel`

## Minimum System Requirements (Windows)

WujihandCpp currently does not support Windows. We plan to add support in the near future.

## Installation

### Docker (Recommended)

We provide a Docker image based on Ubuntu 24.04 and GCC 14 with all required dependencies and toolchains preinstalled. See the [Docker Development Guide](docs/zh-cn/docker-develop-guide.md).

The SDK is preinstalled globally in the image, so you can develop directly inside the container.

Ubuntu 24 includes a recent glibc, and GCC 14 provides full C++20 support; together they ensure optimal performance and an excellent development experience.

### SDK Release Packages

If you prefer not to use Docker, you can install from the release packages available on the [Releases page](https://github.com/Wuji-Technology-Co-Ltd/wujihandcpp/releases).

- Debian/Ubuntu: `sudo apt install ./wujihandcpp-<version>-<arch>.deb`

- Fedora/CentOS/RHEL: `sudo dnf install ./wujihandcpp-<version>-<arch>.rpm`

- Other distributions: manually install headers and libraries from `wujihandcpp-<version>-<arch>.zip`.

### Build from Source

See (TODO).

## Usage

Link against the `wujihandcpp` library.

### CMake

<!-- ```cmake
find_package(wujihandcpp REQUIRED)
target_link_libraries(your_target PRIVATE wujihandcpp::wujihandcpp)
``` -->

```cmake
target_link_libraries(<your_target> PRIVATE wujihandcpp)
```

### Make

```makefile
LDFLAGS += -lwujihandcpp
```

See the `example` directory for usage examples.

## Selected API Reference

### Include headers

```cpp
#include <wujihandcpp/data/data.hpp>   // For data types
#include <wujihandcpp/device/hand.hpp> // For hand device
```

### Connect to the hand

```cpp
wujihandcpp::device::Hand hand{usb_vid, usb_pid};
```

Instantiate a `Hand` object with the USB VID and PID to establish a connection.

In the current firmware, all hands use a fixed VID/PID: VID `0x0483`, PID `0x7530`:

```cpp
wujihandcpp::device::Hand hand{0x0483, 0x7530};
```

### Read data

```cpp
read<typename Data>() -> Data::ValueType;
read<typename Data>() -> void; // For bulk-read
```

All available data types are defined in `wujihandcpp/data/data.hpp`.

For example, read the device uptime (µs):

```cpp
uint32_t time = hand.read<wujihandcpp::data::hand::SystemTime>();
```

In addition to data unique to the whole hand, each joint has its own data types under the `data::joint` namespace.

For example, read the current position of joint 0 of finger 1 (index finger):

```cpp
int32_t position = hand.finger(1).joint(0).read<wujihandcpp::data::joint::Position>();
```

Reading multiple items in a single command is also supported (bulk read).

For example, the following reads the current position of all 20 joints of the hand:

```cpp
hand.read<wujihandcpp::data::joint::Position>();
```

Because multiple items are fetched at once, to avoid unnecessary overhead, `read` returns `void` for bulk reads.

To retrieve the results after a bulk read, call `get`:

```cpp
hand.finger(i).joint(j).get<wujihandcpp::data::joint::Position>();
```

`read` blocks until completion and guarantees success upon return.

Unlike `read`, `get` never blocks; it immediately returns the most recently read data. If no prior read has been requested, the return value is undefined.

### Write data

Writing uses a similar API with an extra parameter for the target value:

```cpp
write<typename Data>(Data::ValueType value) -> void;
```

For example, write the target position of a single joint:

```cpp
hand.finger(1).joint(0).write<wujihandcpp::data::joint::ControlPosition>(0x8FFFFF);
```

Bulk writes are also supported. For example, write the target positions for finger 1:

```cpp
hand.finger(1).write<wujihandcpp::data::joint::ControlPosition>(0x8FFFFF);
```

`write` blocks until completion and guarantees success upon return.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
