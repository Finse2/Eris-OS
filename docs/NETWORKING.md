# ErisOS Networking

Back to [README](README.md)

## Scope

Implemented now:

- RTL8029 PCI detection and NE2000-compatible bring-up
- raw Ethernet TX/RX helpers
- ARP request/reply + cache
- IPv4 framing + checksum validation
- DHCPv4 client flow
- ICMP echo (`PING`)
- UDP socket table + queued receive path
- DNS A-record resolver over UDP
- TCP client state machine + utilities
- simple HTTP over TCP (`HTTPGET`, `TCPGET`)
- desktop Internet app with plain HTTP URL/search loading

Not implemented:

- IPv6
- TCP server/listen path
- TLS/HTTPS
- DNS cache

## Runtime Requirements

QEMU NIC flags used by `compile.bat`:

```bat
-netdev user,id=net0 -device ne2k_pci,netdev=net0
```

If NIC init fails, network commands report not initialized.

Plain web access expects non-HTTPS sites. `http://neverssl.com/` is the default test page because it intentionally serves HTTP.

The web path is still experimental. DNS can fail depending on QEMU networking
state, DHCP results, or resolver behavior. The HTTP tools include a small
fallback for known plain-HTTP test hosts, but HTTPS-only sites are outside the
current design.

## Driver Summary

- PCI config I/O: `0xCF8` / `0xCFC`
- Device: vendor `0x10EC`, device `0x8029`
- RX ring pages: `0x46..0x7F`
- TX start page: `0x40`
- IRQ support: enabled when line is `IRQ11`

## Command Reference

### Link Layer / IP

- `NETINFO`
- `NETTX`
- `NETRX`
- `ARPWHO <IPv4>`
- `ARPCACHE`
- `IPTX <IPv4>`
- `DHCP`
- `PING <IPv4>`

### UDP

- `UDPBIND <port>`
- `UDPSEND <src_port> <dst_ip> <dst_port> <message>`
- `UDPRECV <port>`
- `UDPSOCK`

Limits:

- max bound UDP sockets: `4`
- RX queue entries: `8`
- UDP payload buffer cap: `512` bytes

### DNS

- `DNS <hostname> [dns_server_ip]`

Notes:

- Queries A records only.
- Uses DHCP-provided DNS server by default when available.
- Uses local source port `53053` for resolver traffic.

### TCP

- `TCPCONN <dst_ip> <dst_port>`
- `TCPSEND <text>`
- `TCPRECV`
- `TCPCLOSE`
- `TCPSTAT`
- `TCPGET <dst_ip> <dst_port> [path]`

### HTTP Utility

- `HTTPGET <host_or_ip> [path] [port]`
- `WEB <host_or_url> [path] [port]`

Notes:

- `HTTPGET` accepts hostnames and can invoke DNS.
- `WEB` is a friendlier alias for the same HTTP client path.
- The HTTP path now attempts DHCP automatically once, then falls back to the QEMU user-network defaults.
- `TCPGET` expects destination IP + port explicitly.
- HTTPS/TLS is not implemented, so modern HTTPS-only websites will not load.

## Processing Model

- RX path is interrupt-assisted (when IRQ enabled) and dequeued/polled in shell/protocol handlers.
- Most shell commands are synchronous and print results immediately.
- UDP checksum is transmitted as zero (no checksum generation).
