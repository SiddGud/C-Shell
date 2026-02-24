# Systems Programming Projects Portfolio

This repository contains a collection of systems programming projects demonstrating expertise in Unix programming, network protocols, concurrent programming, and low-level C development. These projects showcase practical implementation skills across different domains of systems software.

## Project Structure

### 1. [Shell Implementation](./shell-implementation/)
Custom Unix-like shell built from scratch in C, implementing core command-line interface features.

**Key Features:**
- **Command Execution** - External program execution with argument handling
- **Pipes (`|`)** - Multi-command pipelines with proper file descriptor management
- **I/O Redirection** - Input (`<`), output (`>`), and append (`>>`) redirection
- **Background Jobs (`&`)** - Non-blocking process execution
- **Job Control** - `fg`, `bg` commands for foreground/background management
- **Signal Handling** - Ctrl+C (SIGINT) and Ctrl+Z (SIGTSTP) support

**Built-in Commands:**
- `hop` - Enhanced cd with previous directory memory (`hop -`)
- `reveal` - Custom ls with long format and hidden files
- `log` - Command history with persistence and re-execution
- `proclore` - Process information display
- `seek` - File/directory search with filters
- `activities` - Background job listing
- `ping` - Send signals to jobs

**Technical Skills:**
- Process management (fork/exec/wait)
- File descriptor manipulation
- Process groups and sessions
- Signal handling
- Persistent state management

### 2. [Network Packet Sniffer](./network-packet-sniffer/)
C-Shark: Terminal-based packet analyzer using libpcap for comprehensive network traffic analysis.

**Capabilities:**
- **Layer-by-Layer Analysis** - Ethernet, IPv4/IPv6, ARP, TCP/UDP, Application
- **Protocol Detection** - HTTP, HTTPS, DNS, FTP, SSH, and more
- **Packet Filtering** - Berkeley Packet Filter support (HTTP, HTTPS, DNS, ARP, TCP, UDP)
- **Session Management** - Store up to 10,000 packets for post-capture inspection
- **Hex Dumps** - Complete packet visualization with ASCII representation
- **Promiscuous Mode** - Capture all network traffic on interface

**Protocol Support:**
- **Ethernet (L2)**: MAC addresses, EtherType identification
- **IPv4/IPv6 (L3)**: Source/destination IPs, TTL, header analysis
- **ARP**: Request/Reply detection, hardware/protocol addresses
- **TCP (L4)**: Ports, sequence numbers, flags (SYN/ACK/FIN/RST/PSH/URG)
- **UDP (L4)**: Ports, checksums, payload
- **Application (L7)**: Service name mapping, payload inspection

**Technical Skills:**
- libpcap programming
- Network byte order conversions (ntohs/ntohl)
- Protocol header parsing
- Pointer arithmetic for header navigation
- Signal handling for graceful interrupts
- Memory management for packet storage

### 3. [Reliable UDP Protocol](./reliable-udp-protocol/)
SHAM (Simple Handshake And Messaging): Custom transport protocol implementing TCP-like reliability on top of UDP.

**Protocol Features:**
- **Connection Management** - Three-way handshake (SYN, SYN-ACK, ACK) for connection establishment
- **Connection Termination** - Four-way handshake for graceful closure
- **Sliding Window Flow Control** - Configurable window size to prevent overwhelming receiver
- **Cumulative Acknowledgments** - Reliable delivery with ACK tracking
- **Automatic Retransmission** - Timeout-based retransmission for lost packets
- **Checksum Validation** - Data integrity verification
- **Out-of-Order Handling** - Receiver buffer for packet reordering

**Application Modes:**
- **File Transfer**: Reliable file transmission over unreliable network
- **Chat**: Interactive messaging with guaranteed delivery

**Testing Features:**
- Configurable packet loss simulation
- Detailed protocol event logging
- Performance metrics collection

**Technical Skills:**
- UDP socket programming
- Reliable protocol design
- Timeout and timer management
- State machine implementation
- Flow control algorithms
- Checksum calculation

### 4. [Synchronization](./synchronization/)
Office Bakery: Multi-threaded simulation demonstrating advanced concurrent programming with complex resource management.

**Problem Domain:**
- **Resources**: 4 ovens, 4 chefs, 4 sofa seats, 25-customer capacity, 1 payment counter
- **Actors**: Multiple customers (threads) and chefs (threads) interacting concurrently
- **Workflow**: Customer entry → sofa seating → cake ordering → payment → exit

**Synchronization Mechanisms:**
- **Mutexes** - Protect shared state (counters, queues, resources)
- **Condition Variables** - Efficient waiting on state changes (customer_cond, chef_cond)
- **State Machine** - Customer progression through bakery states
- **Circular Queues** - FIFO queues for sofa waiting, cake requests, payments
- **Priority Scheduling** - Chef task ordering (payment > baking > seating)

**Key Challenges Solved:**
- Deadlock-free resource allocation
- Priority inversion prevention
- Lost wakeup avoidance with broadcast
- Timed coordination between threads
- Graceful shutdown with active threads

**Technical Skills:**
- POSIX threads (pthreads)
- Mutex and condition variable usage
- Resource pool management
- Producer-consumer patterns
- Circular queue implementation
- Thread lifecycle management

## Technologies & Skills Demonstrated

### Languages & Libraries
- **C** - Primary implementation language for all projects
- **libpcap** - Network packet capture library
- **pthreads** - POSIX threads for concurrent programming

### Core Competencies

**Systems Programming:**
- Low-level C with manual memory management
- Process creation and management (fork/exec/wait)
- File descriptor manipulation and I/O
- Signal handling and process control
- Error handling and resource cleanup

**Network Programming:**
- UDP socket programming (sendto/recvfrom)
- Packet capture with libpcap
- Protocol header parsing (Ethernet, IP, TCP/UDP, ARP)
- Network byte order conversions
- Berkeley Packet Filters

**Concurrent Programming:**
- Thread creation and synchronization
- Mutex-based mutual exclusion
- Condition variables for coordination
- Deadlock prevention strategies
- Resource allocation in multi-threaded environments

**Protocol Design:**
- Reliable transport protocol implementation
- Connection state machines
- Flow control mechanisms
- Timeout and retransmission logic
- Cumulative acknowledgments

## Building and Running

Each subdirectory contains its own detailed README with specific instructions. Quick start:

### Shell Implementation
```bash
cd shell-implementation/
make
./shell
```

### Network Packet Sniffer
```bash
cd network-packet-sniffer/
make
sudo ./cshark  # Requires root for packet capture
```

### Reliable UDP Protocol
```bash
cd reliable-udp-protocol/
make
# Terminal 1:
./server 8080 file output.txt
# Terminal 2:
./client localhost 8080 file input.txt
```

### Synchronization (Bakery)
```bash
cd synchronization/
make
./Bakery < input.txt
```

## Learning Outcomes

Through these projects, I developed expertise in:

1. **Unix System Programming** - Process management, signals, file I/O
2. **Network Programming** - Socket APIs, protocol implementation, packet analysis
3. **Concurrent Programming** - Thread synchronization, deadlock prevention, resource management
4. **Protocol Design** - Implementing reliability on unreliable transport
5. **Low-Level C** - Pointer manipulation, memory management, byte-level operations
6. **Debugging** - Complex system debugging with multiple processes/threads
7. **Performance Analysis** - Understanding trade-offs in different implementations

## Use Cases & Applications

- **Shell**: Understanding command-line interfaces and process control
- **Packet Sniffer**: Network troubleshooting, security analysis, protocol education
- **SHAM Protocol**: Understanding TCP internals, reliable communication
- **Bakery Synchronization**: Learning concurrent programming patterns, resource management

## Key Accomplishments

### Shell Implementation
- Complete pipe implementation with multiple commands
- Proper job control with foreground/background management
- Signal handling that doesn't terminate shell
- Persistent command history across sessions
- 9 built-in commands with Unix-like behavior

### Network Packet Sniffer
- Full OSI layer analysis (L2 through L7)
- Support for IPv4, IPv6, TCP, UDP, ARP, ICMP
- 10,000 packet storage capability
- BPF filter support for targeted capture
- Service name mapping for 20+ common ports

### SHAM Protocol
- Complete connection lifecycle (establish, transfer, teardown)
- Sliding window with configurable size
- Timeout-based retransmission
- Out-of-order packet handling
- Configurable packet loss simulation for testing

### Bakery Synchronization
- Deadlock-free design with 4 resource types
- Priority-based task scheduling for chefs
- Proper state machine for customer workflow
- Graceful shutdown with active threads
- Timing constraints enforced correctly

## Suggested Resume Bullets

- **Systems Programming**: Built Unix shell from scratch with pipes, I/O redirection, job control, and signal handling in C
- **Network Programming**: Developed packet sniffer using libpcap for layer-2 through layer-7 protocol analysis with BPF filtering
- **Protocol Implementation**: Designed and implemented SHAM, a reliable UDP protocol with sliding window flow control and automatic retransmission
- **Concurrent Programming**: Created multi-threaded bakery simulation with mutexes, condition variables, and deadlock-free resource management
- **Low-Level C**: Implemented complex systems with manual memory management, pointer arithmetic, and file descriptor manipulation

---

**Author:** Siddhant Gudwani  
**Purpose:** Portfolio demonstrating systems programming expertise for technical interviews and resume
