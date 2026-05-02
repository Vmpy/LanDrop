# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
# Configure and build (from project root)
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=<Qt6_install_path>
cmake --build build

# Or open CMakeLists.txt in Qt Creator and build with the Desktop kit (MinGW 64-bit)
```

The build output goes to `build/Desktop_Qt_6_11_0_MinGW_64_bit-Debug/` by default via Qt Creator.

## Project Overview

LanDrop is a **LAN file transfer desktop app** — cross-platform (Windows/macOS/Linux), no internet, no accounts. Devices on the same LAN discover each other via UDP multicast and transfer files over TCP.

- **Tech stack**: Qt 6.5+ C++ (Core, Network, Widgets), CMake 3.19+, C++17
- **License constraint**: LGPLv3 dynamic linking — do not use GPL Qt modules
- **UI approach**: C++ code dynamically loads UI; no `.ui` files or QML
- **No external dependencies** beyond Qt built-in modules

## Architecture

**MVC with Qt signal/slot wiring:**

- **`src/core/`** — Network logic & protocol (no GUI dependencies)
  - `discovery_service` — UDP multicast broadcast/listen, maintains device table
  - `transfer_server` — TCP server, accepts incoming connections
  - `file_receiver` — Handles a single incoming file transfer (handshake, chunks, ACK)
  - `file_sender` — Handles a single outgoing file transfer (chunking, retransmit)
  - `protocol` — Packet serialization/deserialization, CRC32 validation
- **`src/ui/`** — Qt Widgets (presentation only, no business logic)
  - `main_window`, `device_list_widget`, `receive_dialog`

**Protocol summary:**
- Discovery: UDP multicast `239.255.255.250:10262`, JSON announce/goodbye, 30s interval, 90s timeout
- Control channel: TCP JSON — handshake → ping/pong heartbeat (15s), send_start, reject
- Data channel: Binary chunks — 4B seq + 4B len + payload + 4B CRC32, 8192-byte chunks, ACK every 64 chunks

Key constants (no magic numbers in code):
- `udp_discovery_port = 10262`, `broadcast_interval_ms = 30000`, `offline_timeout_ms = 90000`
- `heartbeat_interval_ms = 15000`, `heartbeat_timeout_ms = 45000`
- `file_chunk_size = 8192`, `ack_frequency = 64`, `max_retries = 3`

## Coding Conventions

The project follows a strict style (see `docs/编码规范.md` for full details):

- **Brace style**: Allman — braces on their own line always
- **Naming**: ALL lowercase snake_case for everything (classes, functions, variables, enums, namespaces). This is intentional and overrides Qt's CamelCase convention.
- **Member variables**: trailing underscore `_` (e.g., `socket_`, `file_path_`)
- **Indentation**: 4 spaces, no tabs. Max line width 120 chars.
- **Header guards**: `#pragma once` only — no `#ifndef` guards
- **Signal/slot**: New-style syntax only (pointer-to-member), no `SIGNAL()`/`SLOT()` macros
- **No magic numbers**: All port numbers, buffer sizes, timeouts, retry counts, protocol versions must be named `constexpr` constants
- **Namespaces**: All code under `landrop` namespace, with sub-namespaces like `landrop::core`
- **`nullptr`** only, never `NULL` or `0` for null pointers
- **`#include` order**: own header → project headers → Qt headers → std headers (blank line between groups)
- One class per `.cpp`/`.h` pair, filename = class name in snake_case
