# SHAM - Reliable UDP Protocol Implementation

## Overview

SHAM (Simple Handshake And Messaging) is a custom reliable transport protocol built on top of UDP. It implements TCP-like reliability features including connection establishment, flow control, acknowledgments, and retransmission, demonstrating understanding of transport layer protocols and network programming.

## Features

### 1. Connection Management

#### Three-Way Handshake (Connection Establishment)
```
Client                          Server
  |                                |
  |---- SYN (seq=x) ------------->|
  |                                |
  |<--- SYN-ACK (seq=y, ack=x+1)--|
  |                                |
  |---- ACK (ack=y+1) ----------->|
  |                                |
  [Connection Established]
```

#### Four-Way Handshake (Connection Termination)
```
Client                          Server
  |                                |
  |---- FIN (seq=x) ------------->|
  |                                |
  |<--- ACK (ack=x+1) ------------|
  |                                |
  |<--- FIN (seq=y) --------------|
  |                                |
  |---- ACK (ack=y+1) ----------->|
  |                                |
  [Connection Closed]
```

### 2. Reliability Mechanisms

#### Sliding Window Flow Control
- Fixed window size for in-flight packets
- Prevents sender from overwhelming receiver
- Window advancement on acknowledgment
- Configurable window size

#### Cumulative Acknowledgments
- Acknowledges all packets up to highest in-order packet received
- Simplifies receiver buffer management
- Tolerates ACK loss (later ACKs implicitly acknowledge earlier data)

#### Automatic Retransmission
- Timeout-based retransmission for lost packets
- Configurable timeout values
- Exponential backoff (optional enhancement)
- Retransmits only unacknowledged packets

### 3. Data Integrity

#### Checksum Validation
- Computed over packet header and data
- Detects corruption in transit
- Invalid packets silently discarded
- Forces retransmission of corrupted data

### 4. Packet Loss Simulation
- Configurable drop rate for testing
- Simulates unreliable network conditions
- Tests reliability mechanisms under stress
- Helps verify protocol correctness

### 5. Out-of-Order Handling
- Receiver buffer for storing out-of-order packets
- Reorders packets before delivery to application
- Maintains sequence numbers
- Delivers data in correct order to application

### 6. Application Modes

#### File Transfer Mode
```bash
# Server receives and saves file
./server 8080 file output.txt

# Client sends file
./client localhost 8080 file input.txt
```

#### Chat Mode
```bash
# Server in chat mode
./server 8080 chat

# Client in chat mode
./client localhost 8080 chat
```

## Technical Implementation

### Packet Structure

```c
struct sham_packet {
    uint32_t seq_num;        // Sequence number
    uint32_t ack_num;        // Acknowledgment number
    uint16_t flags;          // SYN, ACK, FIN, etc.
    uint16_t window;         // Receiver window size
    uint16_t checksum;       // Data integrity
    uint16_t data_len;       // Payload length
    uint8_t data[MAX_DATA];  // Payload (up to 512 bytes)
};

// Flag bits
#define SYN  0x01    // Synchronize
#define ACK  0x02    // Acknowledgment
#define FIN  0x04    // Finish
#define RST  0x08    // Reset
```

### Connection State Machine

```c
enum connection_state {
    CLOSED,           // No connection
    LISTEN,           // Server waiting for connection
    SYN_SENT,         // Client sent SYN, waiting for SYN-ACK
    SYN_RECEIVED,     // Server received SYN, sent SYN-ACK
    ESTABLISHED,      // Connection active
    FIN_WAIT_1,       // Sent FIN, waiting for ACK
    FIN_WAIT_2,       // Received ACK, waiting for FIN
    TIME_WAIT,        // Waiting before closing
    CLOSE_WAIT,       // Received FIN, waiting to send FIN
    LAST_ACK         // Sent FIN, waiting for ACK
};
```

### Sliding Window Implementation

```c
struct sliding_window {
    uint32_t base;           // Oldest unacknowledged packet
    uint32_t next_seq;       // Next sequence number to send
    uint32_t window_size;    // Maximum packets in flight
    struct packet *buffer;   // Unacknowledged packets
    struct timeval *timers;  // Retransmission timers
};

// Send with flow control
void send_data(char *data, int len) {
    while (next_seq < base + window_size) {
        struct sham_packet pkt;
        pkt.seq_num = next_seq++;
        pkt.data_len = min(len, MAX_DATA);
        memcpy(pkt.data, data, pkt.data_len);
        
        send_packet(&pkt);
        start_timer(pkt.seq_num);
        
        data += pkt.data_len;
        len -= pkt.data_len;
        if (len == 0) break;
    }
}

// Process acknowledgment
void process_ack(uint32_t ack_num) {
    if (ack_num > base) {
        // Slide window forward
        for (uint32_t i = base; i < ack_num; i++) {
            cancel_timer(i);
            free_buffer(i);
        }
        base = ack_num;
    }
}
```

### Receiver Buffer for Out-of-Order Packets

```c
struct receiver_buffer {
    uint32_t expected_seq;        // Next expected sequence number
    struct buffered_packet {
        uint32_t seq_num;
        uint8_t data[MAX_DATA];
        int data_len;
        bool valid;
    } buffer[BUFFER_SIZE];
};

// Handle incoming packet
void receive_packet(struct sham_packet *pkt) {
    if (pkt->seq_num == expected_seq) {
        // In-order packet - deliver immediately
        deliver_to_app(pkt->data, pkt->data_len);
        expected_seq++;
        
        // Check buffer for subsequent packets
        while (buffer[expected_seq % BUFFER_SIZE].valid) {
            deliver_buffered_packet(expected_seq);
            expected_seq++;
        }
    } else if (pkt->seq_num > expected_seq) {
        // Out-of-order packet - buffer it
        int index = pkt->seq_num % BUFFER_SIZE;
        buffer[index] = *pkt;
        buffer[index].valid = true;
    }
    
    // Send cumulative ACK
    send_ack(expected_seq);
}
```

### Timeout and Retransmission

```c
struct timeout_manager {
    uint32_t rto;              // Retransmission timeout (ms)
    struct timeval send_time;  // When packet was sent
};

// Check for timeouts
void handle_timeouts() {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    for (uint32_t seq = base; seq < next_seq; seq++) {
        if (timer_expired(seq, now)) {
            // Retransmit packet
            retransmit_packet(seq);
            // Reset timer (possibly with backoff)
            restart_timer(seq);
        }
    }
}

// Retransmit unacknowledged packet
void retransmit_packet(uint32_t seq_num) {
    struct sham_packet *pkt = get_buffered_packet(seq_num);
    send_packet(pkt);
    log_event("RETRANSMIT", seq_num);
}
```

### Checksum Calculation

```c
uint16_t calculate_checksum(struct sham_packet *pkt) {
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t *)pkt;
    int len = sizeof(struct sham_packet);
    
    // Sum all 16-bit words
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    
    // Add odd byte if present
    if (len == 1) {
        sum += *(uint8_t *)ptr;
    }
    
    // Fold 32-bit sum to 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;  // One's complement
}

bool verify_checksum(struct sham_packet *pkt) {
    uint16_t received = pkt->checksum;
    pkt->checksum = 0;
    uint16_t calculated = calculate_checksum(pkt);
    pkt->checksum = received;
    return received == calculated;
}
```

## Building and Running

### Compilation

```bash
make
```

Or manually:
```bash
gcc -o server server.c -Wall
gcc -o client client.c -Wall
```

### File Transfer Mode

```bash
# Terminal 1: Start server
./server 8080 file output.txt

# Terminal 2: Send file
./client localhost 8080 file input.txt

# Optional: Enable packet loss simulation (10% drop rate)
RUDP_DROP_RATE=10 ./server 8080 file output.txt
```

### Chat Mode

```bash
# Terminal 1: Server
./server 8080 chat
> Hello from server!

# Terminal 2: Client
./client localhost 8080 chat
> Hello from client!
```

### Logging

Enable detailed protocol logging:
```bash
export RUDP_LOG=1
./server 8080 file output.txt
```

Log output:
```
[12:34:56.789] LISTEN: Waiting for connection
[12:34:57.123] RECV: SYN seq=1000
[12:34:57.124] SEND: SYN-ACK seq=2000 ack=1001
[12:34:57.234] RECV: ACK ack=2001
[12:34:57.235] STATE: ESTABLISHED
[12:34:57.345] RECV: DATA seq=1001 len=512
[12:34:57.346] SEND: ACK ack=1513
[12:34:57.456] RECV: DATA seq=1513 len=512
[12:34:57.457] SEND: ACK ack=2025
[12:34:58.567] TIMEOUT: Retransmitting seq=2025
```

## Protocol Design Decisions

### Why Cumulative ACKs?
- Simpler than selective acknowledgments
- Tolerates ACK loss (later ACKs cover earlier data)
- Sufficient for moderate packet loss rates
- Reduces receiver complexity

### Why Fixed Window?
- Predictable behavior for testing
- Simpler than dynamic window adjustment
- Adequate for reliable link with moderate RTT
- Can be extended to dynamic windows later

### Why UDP as Base?
- Demonstrates understanding of building reliability on unreliable transport
- Avoids complexity of raw sockets
- Shows how TCP-like features can be implemented
- Educational value in building from scratch

## Testing

### Test Scenarios

1. **Normal Operation**
   - No packet loss
   - Verify correct data transfer
   - Check connection establishment/teardown

2. **Packet Loss**
   ```bash
   RUDP_DROP_RATE=20 ./server 8080 file output.txt
   ```
   - Verify retransmissions work
   - Check data integrity
   - Confirm no data loss

3. **Out-of-Order Delivery**
   - Simulate network reordering
   - Verify receiver buffer handles it
   - Check sequential delivery to application

4. **Large File Transfer**
   - Test with multi-MB files
   - Verify flow control prevents overwhelming
   - Check performance metrics

5. **Concurrent Connections** (if implemented)
   - Multiple clients to one server
   - Verify isolation between connections

### Validation

```bash
# Send file
./client localhost 8080 file largefile.bin

# Compare checksums
md5sum largefile.bin
md5sum output.txt
# Should match!
```

## Performance Considerations

- **Throughput**: Limited by window size and RTT
  - `Throughput ≈ (Window Size × Packet Size) / RTT`
- **Overhead**: Each packet carries ~16 bytes of header
- **Retransmission Cost**: Doubles bandwidth for lost packets
- **Timeout Tuning**: Too short = unnecessary retransmits, too long = poor performance

## Learning Outcomes

1. **Transport Layer Protocols**: Understanding TCP internals
2. **Reliable Delivery**: Implementing acknowledgments and retransmission
3. **Flow Control**: Sliding window mechanisms
4. **Connection Management**: State machines for connections
5. **Network Programming**: Socket APIs (UDP sendto/recvfrom)
6. **Timing**: Timeouts and timer management
7. **Data Integrity**: Checksum algorithms
8. **Buffering**: Managing out-of-order data

## Potential Enhancements

- **Dynamic Window Sizing**: Adjust based on network conditions
- **Selective ACKs (SACK)**: Acknowledge specific packets, not just cumulative
- **Fast Retransmit**: Retransmit on 3 duplicate ACKs
- **Congestion Control**: Slow start, congestion avoidance
- **RTT Estimation**: Dynamic timeout calculation
- **Pipelining**: Send multiple packets before waiting for ACKs

## References

- RFC 793: Transmission Control Protocol (TCP)
- RFC 768: User Datagram Protocol (UDP)
- "Computer Networks" by Tanenbaum
- TCP/IP Illustrated by Stevens
- Go-Back-N and Selective Repeat ARQ protocols

## Files

- `sham.h` - Protocol definitions and structures
- `server.c` - Server implementation
- `client.c` - Client implementation
- `Makefile` - Build configuration
- `README.md` - This documentation

---

SHAM demonstrates practical understanding of transport protocols by implementing TCP-like reliability features on top of unreliable UDP, showing mastery of network programming and protocol design.
