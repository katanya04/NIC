# Networking (Raw Ethernet NIC demo)

A small C project that implements a simple NIC abstraction (`interface.c`) on top of a Linux raw-socket hardware abstraction layer (`hal.c`). The included `main.c` builds and sends a test Ethernet frame and prints any received packets via a callback.

## What it does

- Opens a Linux `AF_PACKET` / `SOCK_RAW` socket bound to an interface (default: `eth0`).
- Spawns a background thread that:
  - Reads frames from the raw socket and fires RX callbacks.
  - Flushes queued TX buffers out to the raw socket and fires TX callbacks.
- The demo program (`main.c`) constructs an Ethernet frame with a test EtherType (`0x9000`) and sends it to the broadcast MAC.

## Project layout

- `hal.c` / `hal.h`
  - Linux-specific raw socket access: create/remove device, send/receive, get MAC address, get MTU.
- `interface.c` / `interface.h`
  - A basic NIC driver API:
    - `nic_init`, `nic_shutdown`
    - `nic_send_packet`, `nic_receive_packet`
    - `nic_ioctl` for callbacks, MTU, MAC, stats, up/down
  - Background processing thread that bridges RX/TX to the HAL.
- `main.c`
  - Demo app: initializes NIC, registers RX callback, sends one test Ethernet frame, waits for Enter, then shuts down.

## Requirements

- Linux
- A network interface you can bind to (default: `eth0`)
- Raw sockets require elevated privileges:
  - run as root (`sudo`), or
  - grant capability: `sudo setcap cap_net_raw+ep bin/networking`

## Build

From the project directory:

```bash
make
```

## Run

```bash
sudo bin/networking
```

You should see something like:

- A prompt to press Enter to exit
- Any incoming frames printed as: `Received packet of length ... [.. .. ..]`

Note: On a busy interface, you may receive many frames.

## Configure interface name

The interface is currently hard-coded in `hal.h`:

```c
#define HAL_IFACE_NAME "eth0"
```

Change it to the interface you want (e.g. `"enp0s3"`, `"wlan0"`), rebuild, and run again.

## Notes / limitations

- `main.c` builds a test Ethernet frame with a hard-coded payload size and uses a simplified frame struct.
- This is a learning/demo project and not a full-featured NIC stack (no ARP/IP/TCP/UDP parsing, no filtering, no checksum/CRC handling beyond what the kernel/NIC does).
- RX callbacks are invoked from the NIC worker thread.

## Useful next steps

- Add filtering (e.g., only print frames matching a specific EtherType).
- Improve TX/RX buffer management (locking, backpressure, and cleanup on errors).
