# SimpleRFCOMM Installation and Platform Setup Guide

This guide describes how to install prerequisites, configure the Bluetooth stack, build the project, and integrate **SimpleRFCOMM** into your C++20 applications on both **Windows** and **Linux** (including NVIDIA Jetson Orin / Raspberry Pi).

---

## Table of Contents
1. [Prerequisites](#1-prerequisites)
2. [Linux Setup & BlueZ SDP Configuration](#2-linux-setup--bluez-sdp-configuration)
3. [Windows Setup](#3-windows-setup)
4. [Building SimpleRFCOMM](#4-building-simplerfcomm)
5. [Integrating into Your CMake Project](#5-integrating-into-your-cmake-project)
6. [Troubleshooting & FAQ](#6-troubleshooting--faq)

---

## 1. Prerequisites

### Windows
- **OS**: Windows 10 (version 1809+) or Windows 11 (x64 / ARM64).
- **Compiler**:
  - Microsoft Visual Studio 2022 (Community, Professional, or Build Tools) with the **Desktop development with C++** workload (MSVC v143+), OR
  - LLVM Clang 16+.
- **Windows SDK**: Version 10.0.22000.0 or newer.
- **Build System**: CMake 3.20 or newer.
- **Hardware**: Built-in or USB Bluetooth adapter supporting Bluetooth 2.1+EDR, 4.x, or 5.x.

### Linux (Ubuntu / Debian / NVIDIA Jetson Orin / Raspberry Pi)
- **OS**: Ubuntu 20.04 / 22.04 / 24.04, Debian 11/12, or NVIDIA JetPack (L4T 35.x / 36.x).
- **Compiler**: GCC 11+ or Clang 14+ supporting C++20.
- **Build Tools**: `cmake`, `make` or `ninja`, `pkg-config`.
- **Bluetooth Packages**: `bluez`, `libbluetooth-dev`.

To install dependencies on Linux:
```bash
sudo apt update
sudo apt install -y build-essential cmake libbluetooth-dev bluez
```

---

## 2. Linux Setup & BlueZ SDP Configuration

### Enabling BlueZ SDP Compatibility Mode (`--compat`)

By default in BlueZ 5, the Bluetooth daemon `bluetoothd` disables legacy SDP registration, meaning the Unix socket `/var/run/sdp` is not created. Without this socket, non-root user applications cannot publish SDP service records.

To enable the SDP daemon socket, add the `--compat` (`-C`) flag and grant read/write permissions to `/var/run/sdp`.

Run this script once on your Linux machine (or Jetson Orin):

```bash
sudo bash -c 'mkdir -p /etc/systemd/system/bluetooth.service.d
cat > /etc/systemd/system/bluetooth.service.d/zz-compat.conf << "EOF"
[Service]
ExecStart=
ExecStart=/usr/lib/bluetooth/bluetoothd -d -C --noplugin=audio,a2dp,avrcp
ExecStartPost=-/bin/chmod 777 /var/run/sdp
EOF
systemctl daemon-reload
systemctl restart bluetooth
'
```

Verify that `/var/run/sdp` exists:
```bash
ls -la /var/run/sdp
# Expected output:
# srwxrwxrwx 1 root root 0 Sep 12 12:00 /var/run/sdp
```

### Making the Linux Bluetooth Adapter Discoverable & Pairable

Open the Bluetooth control utility:
```bash
bluetoothctl
```
Inside the interactive prompt (or via single commands):
```text
power on
discoverable-timeout 0
discoverable on
pairable on
agent NoInputNoOutput
default-agent
quit
```

Or run via command line:
```bash
bluetoothctl power on
bluetoothctl discoverable-timeout 0
bluetoothctl discoverable on
bluetoothctl pairable on
```

---

## 3. Windows Setup

1. **Turn on Bluetooth**:
   Open **Settings -> Bluetooth & devices** and ensure Bluetooth is turned **On**.
2. **Make Windows Discoverable**:
   Keeping the **Settings -> Bluetooth & devices** window open automatically makes the Windows PC discoverable to nearby Bluetooth devices (such as Jetson Orin).
3. **Check Local Bluetooth MAC Address**:
   You can view your Bluetooth adapter's physical MAC address in PowerShell:
   ```powershell
   Get-NetAdapter | Where-Object { $_.InterfaceDescription -like "*Bluetooth*" } | Format-List
   ```
   Or run the provided `client_bench` or `scan_win` tools.

---

## 4. Building SimpleRFCOMM

### Building on Windows (MSVC)

```powershell
# Clone or navigate to the repository
cd D:\Sources\Giit\SimpleRFCOMM

# Generate Visual Studio 2022 project files
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build in Release mode
cmake --build build --config Release
```

The compiled binaries will be placed in `build\Release\`:
- `simple_rfcomm.lib` — Static library
- `server_bench.exe` — Benchmark server
- `client_bench.exe` — Benchmark client
- `chat_echo.exe` — Interactive chat application

### Building on Linux / NVIDIA Jetson Orin

```bash
cd /path/to/SimpleRFCOMM

# Create build directory
mkdir -p build && cd build

# Configure and compile with all CPU cores
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

The compiled binaries will be placed directly in `build/`:
- `libsimple_rfcomm.a` — Static library
- `server_bench` — Benchmark server
- `client_bench` — Benchmark client
- `chat_echo` — Interactive chat application

---

## 5. Integrating into Your CMake Project

### Option A: `add_subdirectory` (Recommended)

Include the `SimpleRFCOMM` repository inside your project (e.g. in `third_party/SimpleRFCOMM` or as a Git submodule):

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyBluetoothApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add SimpleRFCOMM subdirectory
add_subdirectory(third_party/SimpleRFCOMM)

# Your executable
add_executable(my_app src/main.cpp)

# Link against simple_rfcomm (headers and OS libs are linked automatically)
target_link_libraries(my_app PRIVATE simple_rfcomm)
```

### Option B: CMake Installation (`find_package`)

1. Install `SimpleRFCOMM` system-wide or to a local prefix:
   ```bash
   # On Linux:
   cd build && sudo make install

   # On Windows:
   cmake --install build --prefix "C:/libs/SimpleRFCOMM"
   ```
2. Link in your application `CMakeLists.txt`:
   ```cmake
   find_package(SimpleRFCOMM REQUIRED)
   target_link_libraries(my_app PRIVATE SimpleRFCOMM::simple_rfcomm)
   ```

---

## 6. Troubleshooting & FAQ

### Q: On Windows, `connect()` fails with error `10051 (WSAENETUNREACH)`
- **Cause**: The remote device is either out of range, not powered on, or not in discoverable mode.
- **Solution**:
  1. On Linux/Jetson, run `bluetoothctl discoverable on` and `bluetoothctl pairable on`.
  2. Test low-level link connectivity from Linux using `sudo l2ping -c 3 <WINDOWS_MAC>`.

### Q: On Windows, `bind()` fails with error `10013 (WSAEACCES)`
- **Cause**: RFCOMM port 1 (and certain low channels) are often reserved by Windows system profiles (such as dial-up networking or serial ports).
- **Solution**:
  Use `channel = 0` (or `BT_PORT_ANY`). Winsock will automatically assign an available RFCOMM port (typically 2..30), and Bluetooth SDP will register this assigned port for clients to discover.

### Q: On Linux, `sdp_connect()` fails with "No such file or directory"
- **Cause**: The BlueZ daemon is running without `--compat`, so `/var/run/sdp` does not exist.
- **Solution**: Follow [Section 2: Enabling BlueZ SDP Compatibility Mode](#enabling-bluez-sdp-compatibility-mode---compat).

### Q: Throughput is low (< 20 KB/s)
- **Cause**: Sending very small buffers (e.g. 10–50 bytes) incurs severe Bluetooth packet header overhead.
- **Solution**: Send data in **16 KB chunks** using `sock->send_all()` to fully saturate the Bluetooth 3-DH5 baseband frames. Verify that `SO_SNDBUF` and `SO_RCVBUF` are configured to 512 KB (applied automatically by `SimpleRFCOMM`).
