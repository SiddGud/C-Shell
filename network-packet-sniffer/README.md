# C-Shark Network Packet Sniffer

## Overview

C-Shark is a terminal-based network packet sniffer and analyzer built using the libpcap library in C. It captures live network traffic and provides detailed layer-by-layer analysis of packets, from Ethernet frames up through application protocols.

## Features

### Core Capabilities

1. **Interface Selection**
   - Discovers all available network interfaces
   - Displays interface descriptions
   - User selects interface to monitor

2. **Packet Capture Modes**
   - **All Packets**: Capture without filtering
   - **Filtered Capture**: Filter by protocol (HTTP, HTTPS, DNS, ARP, TCP, UDP)

3. **Layer-by-Layer Analysis**
   - **Layer 2 (Data Link)**: Ethernet frame analysis
   - **Layer 3 (Network)**: IPv4, IPv6, ARP
   - **Layer 4 (Transport)**: TCP, UDP
   - **Layer 7 (Application)**: HTTP, HTTPS, DNS identification

4. **Session Management**
   - Store up to 10,000 packets from last session
   - Inspect captured packets after session ends
   - View packet summaries and detailed breakdowns

5. **Advanced Features**
   - Live hex dump of payload (first 64 bytes)
   - Full packet hex dump on inspection
   - Service name mapping for common ports
   - Signal handling (Ctrl+C stops capture, returns to menu)
   - Promiscuous mode for all network traffic

## Protocol Support

### Ethernet (Layer 2)
- Source/Destination MAC addresses
- EtherType identification (IPv4, IPv6, ARP)

### IPv4 (Layer 3)
- Source/Destination IP addresses
- Protocol type (TCP, UDP, ICMP, etc.)
- TTL, header length, total length
- Packet ID, flags, fragment offset

### IPv6 (Layer 3)
- Source/Destination IPv6 addresses
- Next header (protocol)
- Hop limit, traffic class, flow label
- Payload length

### ARP (Layer 2/3)
- Operation type (Request/Reply)
- Sender/Target MAC addresses
- Sender/Target IP addresses
- Hardware and protocol types

### TCP (Layer 4)
- Source/Destination ports
- Service name mapping (HTTP, HTTPS, SSH, FTP, DNS, etc.)
- Sequence/Acknowledgment numbers
- TCP flags (SYN, ACK, FIN, RST, PSH, URG)
- Window size, checksum
- TCP header length

### UDP (Layer 4)
- Source/Destination ports
- Service name mapping
- UDP length and checksum
- Payload analysis

### Application Layer (Layer 7)
- HTTP detection (port 80)
- HTTPS detection (port 443)
- DNS detection (port 53)
- Payload hex dump (16 bytes per line)
- ASCII representation

## Technical Implementation

### Architecture

```c
// Packet storage structure
struct StoredPacket {
    int id;                          // Packet sequence number
    struct timeval timestamp;        // Capture time
    uint32_t length;                 // Packet length
    uint8_t *data;                   // Complete packet data
};

// Global storage
StoredPacket stored_packets[10000];
int packet_count = 0;
```

### Key Technologies

1. **libpcap Library**
   - `pcap_findalldevs()`: Interface discovery
   - `pcap_open_live()`: Open interface for capture
   - `pcap_loop()`: Packet capture loop
   - `pcap_compile()` / `pcap_setfilter()`: Berkeley Packet Filters
   - `pcap_breakloop()`: Stop capture on signal

2. **Network Headers**
   - `<net/ethernet.h>`: Ethernet structures
   - `<netinet/ip.h>`: IPv4 structures
   - `<netinet/ip6.h>`: IPv6 structures
   - `<netinet/tcp.h>`: TCP structures
   - `<netinet/udp.h>`: UDP structures
   - `<net/if_arp.h>`: ARP structures

3. **Byte Order Conversion**
   - `ntohs()`: Network to host short (16-bit)
   - `ntohl()`: Network to host long (32-bit)
   - `inet_ntop()`: Binary to presentation (IP addresses)

### Packet Processing Flow

```
Capture Packet
    ↓
Extract Ethernet Header
    ↓
Check EtherType → IPv4 / IPv6 / ARP
    ↓
Extract Network Layer Header
    ↓
Check Protocol → TCP / UDP / ICMP
    ↓
Extract Transport Layer Header
    ↓
Identify Application Protocol (by port)
    ↓
Display Layer Information + Hex Dump
    ↓
Store Packet in Memory
```

### Signal Handling

```c
volatile sig_atomic_t stop_sniffing = 0;
pcap_t *global_handle = NULL;

void sigint_handler(int signum) {
    stop_sniffing = 1;
    if (global_handle) {
        pcap_breakloop(global_handle);
    }
}

// Register handler
signal(SIGINT, sigint_handler);
```

### Berkeley Packet Filter Syntax

| Filter | BPF Expression |
|--------|---------------|
| HTTP | `tcp port 80` |
| HTTPS | `tcp port 443` |
| DNS | `port 53` |
| ARP | `arp` |
| TCP | `tcp` |
| UDP | `udp` |

## Building and Running

### Prerequisites

```bash
# Install libpcap development library
# Ubuntu/Debian:
sudo apt-get install libpcap-dev

# Fedora/RHEL:
sudo dnf install libpcap-devel

# macOS:
brew install libpcap
```

### Compilation

```bash
make
```

Or manually:
```bash
gcc -o cshark cshark.c -lpcap -Wall
```

### Execution

**Requires root privileges** (for packet capture):

```bash
sudo ./cshark
```

### Usage Flow

1. **Select Interface**
   ```
   Available Interfaces:
   1. eth0 (Ethernet)
   2. wlan0 (Wireless)
   3. lo (Loopback)
   Select interface: 1
   ```

2. **Main Menu**
   ```
   1. Start sniffing (All packets)
   2. Start sniffing (Filter)
   3. Inspect last session
   4. Exit
   Choice: 1
   ```

3. **Capture Packets**
   - Press **Ctrl+C** to stop capture
   - Returns to main menu

4. **Inspect Session**
   ```
   Packet Summary:
   [1] 10:23:45.123456 - 74 bytes - IPv4 192.168.1.5:443 -> 192.168.1.100:54321
   [2] 10:23:45.234567 - 60 bytes - ARP Request
   ...
   
   Enter packet ID to inspect (0 to return): 1
   ```

## Example Output

### Ethernet Header
```
=== Ethernet Header ===
Source MAC: 00:11:22:33:44:55
Destination MAC: AA:BB:CC:DD:EE:FF
EtherType: IPv4 (0x0800)
```

### IPv4 Header
```
=== IPv4 Header ===
Source IP: 192.168.1.100
Destination IP: 93.184.216.34
Protocol: TCP (6)
TTL: 64
Header Length: 20 bytes
Total Length: 52 bytes
Packet ID: 12345
```

### TCP Header
```
=== TCP Header ===
Source Port: 54321 (Unknown)
Destination Port: 443 (HTTPS)
Sequence Number: 1234567890
Acknowledgment Number: 987654321
Flags: [ACK]
Window Size: 65535
TCP Header Length: 20 bytes
Payload Offset: 54
Payload Length: 0
```

### Application Layer
```
=== Application Layer ===
Protocol: HTTPS
Payload Length: 1024 bytes

Hex Dump (first 64 bytes):
0000: 17 03 03 04 00 a1 b2 c3 d4 e5 f6 07 18 29 3a 4b  ............):K
0010: 5c 6d 7e 8f 90 a1 b2 c3 d4 e5 f6 07 18 29 3a 4b  \m~..........):K
0020: 5c 6d 7e 8f 90 a1 b2 c3 d4 e5 f6 07 18 29 3a 4b  \m~..........):K
0030: 5c 6d 7e 8f 90 a1 b2 c3 d4 e5 f6 07 18 29 3a 4b  \m~..........):K
```

## Port to Service Mapping

| Port | Service | Protocol |
|------|---------|----------|
| 20, 21 | FTP | TCP |
| 22 | SSH | TCP |
| 23 | Telnet | TCP |
| 25 | SMTP | TCP |
| 53 | DNS | TCP/UDP |
| 80 | HTTP | TCP |
| 110 | POP3 | TCP |
| 143 | IMAP | TCP |
| 443 | HTTPS | TCP |
| 3306 | MySQL | TCP |
| 5432 | PostgreSQL | TCP |
| 6379 | Redis | TCP |
| 8080 | HTTP Alt | TCP |

## Key Concepts Demonstrated

1. **Network Programming**
   - Raw packet capture using libpcap
   - Interface management
   - Promiscuous mode operation

2. **Protocol Analysis**
   - Header parsing and interpretation
   - Multi-layer protocol dissection
   - EtherType and protocol number handling

3. **Memory Management**
   - Dynamic packet storage allocation
   - Session-based memory cleanup
   - Buffer overflow prevention

4. **Byte Manipulation**
   - Pointer arithmetic for header navigation
   - Network byte order conversion
   - Hexadecimal and ASCII formatting

5. **Signal Handling**
   - Graceful interrupt handling
   - Resource cleanup on signal
   - Safe signal handler implementation

6. **Binary Data Processing**
   - Raw byte interpretation
   - Bitwise operations for flags
   - Binary to human-readable conversion

## Use Cases

- **Network Troubleshooting**: Diagnose connectivity issues
- **Security Analysis**: Monitor suspicious traffic
- **Protocol Learning**: Understand packet structures
- **Performance Analysis**: Analyze traffic patterns
- **Development Testing**: Debug network applications

## Limitations

- Requires root/administrator privileges
- Single interface capture at a time
- Limited to 10,000 packet storage
- No packet injection or modification
- Hex dump limited to 64 bytes in live capture

## Future Enhancements

Possible additions:
- Multiple interface support
- Packet statistics (bandwidth, protocols, top talkers)
- Export to PCAP file format
- Real-time filtering during capture
- Full payload inspection
- Protocol-specific parsing (HTTP headers, DNS queries)
- TCP stream reconstruction
- Graphical visualization

## Security Note

This tool captures all network traffic including potentially sensitive information. Use responsibly and only on networks you have permission to monitor. Capturing others' traffic without authorization may violate laws and regulations.

## Files

- `cshark.c` - Complete implementation
- `Makefile` - Build configuration
- `README.md` - This documentation

## References

- libpcap Documentation: https://www.tcpdump.org/
- Wireshark: Reference implementation of packet analysis
- RFC 791 (IPv4), RFC 2460 (IPv6)
- RFC 793 (TCP), RFC 768 (UDP)
- RFC 826 (ARP)
- Network protocol specifications

---

C-Shark demonstrates practical network programming skills with real-world packet analysis capabilities suitable for network administration, security analysis, and protocol education.
