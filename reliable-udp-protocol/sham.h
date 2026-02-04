/* ############## LLM Generated Code Begins ############## */
#ifndef SHAM_H
#define SHAM_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <openssl/md5.h>
#include <errno.h>

// --- Protocol Constants ---
#define DATA_CHUNK_SIZE 1024
#define SENDER_WINDOW_PACKETS 10 // Fixed sender window size in packets
#define RTO_MS 500               // Retransmission Timeout in milliseconds
#define RECEIVER_BUFFER_SIZE (1024 * 1024) // 1MB receiver buffer

// --- S.H.A.M. Flags ---
#define SYN_FLAG 0x1
#define ACK_FLAG 0x2
#define FIN_FLAG 0x4

// --- S.H.A.M. Packet Header Structure ---
// Using __attribute__((packed)) to prevent padding and ensure consistent size
struct sham_header {
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t flags;
    uint16_t window_size; // Receiver's available buffer space
};

// --- Full S.H.A.M. Packet Structure ---
struct sham_packet {
    struct sham_header hdr;
    char data[DATA_CHUNK_SIZE];
};

// --- **NEW**: Moved here for shared use in chat mode ---
// Structure to manage sent packets for the sliding window
typedef struct {
    struct sham_packet packet;
    struct timeval send_time;
    int data_len;
    int acked;
} sent_packet_info;


// --- Global Variables for Logging ---
extern FILE* log_file;
extern int logging_enabled;

// --- Function Prototypes for Utilities ---

/**
 * @brief Initializes the logging system.
 * Checks for the RUDP_LOG environment variable and opens the specified log file.
 * @param filename The name of the log file ("client_log.txt" or "server_log.txt").
 */
void setup_logging(const char* filename);

/**
 * @brief Logs a formatted message to the log file if logging is enabled.
 * Prepends a high-precision timestamp to each message.
 * @param format The format string for the log message (like printf).
 * @param ... Variable arguments for the format string.
 */
void log_event(const char* format, ...);

/**
 * @brief Calculates the MD5 checksum of a file.
 * @param filename The path to the file.
 * @param md5_str A buffer to store the resulting 32-character lowercase hex string.
 * @return 0 on success, -1 on failure.
 */
int calculate_md5(const char* filename, char* md5_str);

/**
 * @brief Returns the current time as a struct timeval.
 */
struct timeval get_current_time();

/**
 * @brief Calculates the time difference in milliseconds.
 * @param end The end time.
 * @param start The start time.
 * @return The difference in milliseconds.
 */
long time_diff_ms(struct timeval end, struct timeval start);


#endif // SHAM_H
/* ############## LLM Generated Code Ends ################ */