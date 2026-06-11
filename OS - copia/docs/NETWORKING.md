# ErisOS Networking

> Implementation details for ErisOS networking (RTL8029 / NE2000).
>
> Back to [README](README.md)

---

## Scope

Current networking in ErisOS is early-stage L2 + minimal L3/L4:
- PCI detection of RTL8029 (`vendor 0x10EC`, `device 0x8029`)
- NE2000-compatible register setup and ring buffer configuration
- Raw Ethernet transmit (`NETTX`)
- IRQ-driven RX completion + shell-level dequeue (`NETRX`)
- Polling-based TX completion for stability
- ARP request/reply handling + ARP cache
- Minimal IPv4 framing/parsing with header checksum validation
- DHCPv4 client (DISCOVER/OFFER/REQUEST/ACK)
- ICMP echo request/reply (`PING`)
- DNS resolver command (`DNS`) for A-record lookups over UDP
- UDP shell API (`UDPBIND`, `UDPSEND`, `UDPRECV`, `UDPSOCK`)
- Expanded TCP client path (`TCPCONN`, `TCPSEND`, `TCPRECV`, `TCPCLOSE`, `TCPSTAT`, `TCPGET`)

Not implemented yet:
- IPv6
- TCP listen/accept server-side sockets
- DNS caching and hostname support across other commands

---

## Runtime Environment

QEMU must expose an NE2000-compatible PCI NIC:

```bash
qemu-system-i386 ... -netdev user,id=net0 -device ne2k_pci,netdev=net0
```

At boot, `start()` in `main.c`:
1. Calls `ne2k_init()`
2. Enables IRQ-driven receive path on NIC IRQ11 (when available)
3. Attempts DHCP lease acquisition

If NIC init fails, networking commands report NIC not initialized.

---

## Driver Architecture

Files:
- `ne2000.c`: low-level PCI + NE2000 driver logic
- `ne2000.h`: public driver API
- `extra.h`: shell-level protocol handling and commands

I/O model:
- PCI config mechanism #1 ports: `0xCF8`, `0xCFC`
- NE2000 register window at BAR0-derived `io_base`
- IMR set for RX IRQs (`PRX/RXE/OVW`)
- TX completion checked by polling NIC status bits (stability-oriented)

---

## Initialization Flow

`ne2k_init()` performs:
1. PCI scan for RTL8029.
2. Enable PCI I/O space + bus mastering.
3. Read BAR0 (`g_io_base`) and IRQ line (`g_irq_line`).
4. Hardware reset via `NE_RESET`.
5. Program TX/RX ring layout and runtime registers.
6. Read/program MAC address.
7. Validate MAC is not all `00`/`FF`.
8. Mark NIC ready.

---

## NE2000 Memory Layout

Configured NIC RAM pages:
- TX start page: `0x40`
- TX page count: `0x06`
- RX start page: `0x46`
- RX stop page: `0x80`

Runtime mode:
- `RCR = 0x04` (accept broadcasts)
- `TCR = 0x00` (normal transmit)
- `IMR = PRX/RXE/OVW` (receive interrupts enabled)

---

## Shell Commands

### `NETINFO`
- Shows NIC model, I/O base, IRQ, MAC, IP, subnet mask, gateway, DNS, DHCP state.

### `NETTX`
- Sends one broadcast Ethernet test frame (`EtherType 0x88B5`).

### `NETRX`
- Polls/dequeues one frame and dumps first bytes.

### `ARPWHO <IPv4>`
- ARP-resolves target IP to MAC (with timeout polling).

### `ARPCACHE`
- Shows learned ARP cache entries.

### `IPTX <IPv4>`
- Sends one minimal IPv4 test packet (`proto 0xFD`).

### `DHCP`
- Requests/renews DHCP lease and updates IP/mask/gateway/DNS/lease.

### `PING <ip>`
- Sends ICMP Echo Request and waits for Echo Reply.
- Prints reply source, sequence, and RTT.

### `UDPBIND <port>`
- Binds a local UDP port in the socket table.

### `UDPSEND <src_port> <dst_ip> <dst_port> <message>`
- Sends a UDP datagram from a bound local port.

### `UDPRECV <port>`
- Receives one queued UDP datagram for a bound port.

### `UDPSOCK`
- Lists bound UDP ports.

### `TCPGET <ip> <port> [path]`
- Minimal TCP client utility:
  - SYN -> SYN/ACK -> ACK handshake
  - Sends HTTP/1.0 GET request
  - Polls/prints received response bytes

### `TCPCONN <ip> <port>`
- Opens a TCP client connection.

### `TCPSEND <text>`
- Sends payload over the established TCP connection.

### `TCPRECV`
- Prints and clears buffered TCP receive data.

### `TCPCLOSE`
- Gracefully closes active TCP connection.

### `TCPSTAT`
- Shows TCP state and sequence/counter info.

### `DNS <hostname> [dns_server_ip]`
- Sends a UDP DNS query (`QTYPE=A`) to port `53`.
- Uses configured `net_dns_ip` by default, or explicit server IP argument.
- Waits synchronously for response and prints resolved IPv4 address.

### `HTTPGET <host_or_ip> [path] [port]`
- Resolves hostname via DNS when needed.
- Opens TCP connection to target (default port `80`).
- Sends HTTP/1.0 GET request with `Host` header.
- Prints raw HTTP response bytes.

---

## RX/Protocol Processing

Receive flow:
1. IRQ11 handler calls `ne2k_handle_irq()` to ack RX status.
2. Shell/protocol code dequeues packets via `ne2k_poll_receive()`.
3. Protocol dispatch:
   - ARP (`0x0806`)
   - IPv4 (`0x0800`)
4. IPv4 dispatch:
   - ICMP (`proto 1`) echo handling
   - UDP (`proto 17`) queue delivery + DHCP parsing on `67 -> 68` + DNS response handling on `53 -> 53053`
   - TCP (`proto 6`) client state machine with ACK tracking, retransmit timer, and close states

Transmit flow:
- Frames are sent by `ne2k_send_frame()`.
- TX completion is confirmed by polling NIC ISR bits.

---

## Current Limitations

- TCP is client-only (no passive listen/accept yet).
- DNS currently resolves A records only (no AAAA/CNAME chain handling).
- HTTP client is plain HTTP only (no TLS/HTTPS).
- UDP checksum is not generated/verified (set to `0` on transmit).
- No fragmentation reassembly.
- Shell-level networking APIs are synchronous/poll-driven.

---

## Suggested Next Milestones

1. DHCP renew/rebind timers (T1/T2) with automatic refresh.
2. Expand TCP (retransmits/windowing/robust close states).
3. Add DNS cache + hostname support across client commands (`TCPGET`, future tools).
