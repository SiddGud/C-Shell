/* ############## LLM Generated Code Begins ############## */
#include "sham.h"
#include <sys/select.h>
#include <stdarg.h>
#include <stdbool.h> // For bool type

// Global log file pointer
FILE* log_file = NULL;
int logging_enabled = 0;

void setup_logging(const char* filename) {
    if (getenv("RUDP_LOG")) {
        logging_enabled = 1;
        log_file = fopen(filename, "w");
        if (!log_file) {
            perror("Failed to open log file");
            exit(EXIT_FAILURE);
        }
    }
}

void log_event(const char* format, ...) {
    if (!logging_enabled || !log_file) return;

    char time_buffer[30];
    struct timeval tv;
    time_t curtime;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;
    strftime(time_buffer, 30, "%Y-%m-%d %H:%M:%S", localtime(&curtime));

    fprintf(log_file, "[%s.%06ld] [LOG] ", time_buffer, tv.tv_usec);

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fflush(log_file);
}

int calculate_md5(const char* filename, char* md5_str) {
    unsigned char c[MD5_DIGEST_LENGTH];
    FILE *inFile = fopen(filename, "rb");
    if (inFile == NULL) {
        printf("Error: Cannot open file %s for MD5 calculation.\n", filename);
        return -1;
    }

    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];

    MD5_Init(&mdContext);
    while ((bytes = fread(data, 1, 1024, inFile)) != 0) {
        MD5_Update(&mdContext, data, bytes);
    }
    MD5_Final(c, &mdContext);

    for(int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(&md5_str[i*2], "%02x", (unsigned int)c[i]);
    }

    fclose(inFile);
    return 0;
}

struct timeval get_current_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv;
}

long time_diff_ms(struct timeval end, struct timeval start) {
    return (end.tv_sec - start.tv_sec) * 1000L + (end.tv_usec - start.tv_usec) / 1000L;
}

// Returns 0 to continue server, 1 to shut down
int handle_file_transfer(int sockfd, struct sockaddr_in* client_addr, socklen_t* addr_len, float loss_rate) {
    char output_filename[256];
    uint32_t next_expected_seq = 0;

    struct sham_packet packet;
    int n = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr*)client_addr, addr_len);
    if (n > 0) {
        strncpy(output_filename, packet.data, n - sizeof(struct sham_header));
        output_filename[n - sizeof(struct sham_header)] = '\0';
        next_expected_seq = packet.hdr.seq_num + (n - sizeof(struct sham_header));
        log_event("RCV DATA SEQ=%u LEN=%ld (Filename: %s)\n", packet.hdr.seq_num, n - sizeof(struct sham_header), output_filename);

        struct sham_packet ack_packet;
        memset(&ack_packet, 0, sizeof(ack_packet));
        ack_packet.hdr.flags = ACK_FLAG;
        ack_packet.hdr.ack_num = next_expected_seq;
        ack_packet.hdr.window_size = RECEIVER_BUFFER_SIZE;
        sendto(sockfd, &ack_packet, sizeof(struct sham_header), 0, (struct sockaddr*)client_addr, *addr_len);
        log_event("SND ACK=%u WIN=%u\n", ack_packet.hdr.ack_num, ack_packet.hdr.window_size);
    } else {
        perror("recvfrom filename");
        return 0;
    }

    FILE* fp = fopen(output_filename, "wb");
    if (!fp) {
        perror("Failed to open output file");
        return 0;
    }

    while (1) {
        n = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr*)client_addr, addr_len);
        if (n <= 0) continue;

        if (packet.hdr.flags & FIN_FLAG) {
            log_event("RCV FIN SEQ=%u\n", packet.hdr.seq_num);
            struct sham_packet ack_packet;
            memset(&ack_packet, 0, sizeof(ack_packet));
            ack_packet.hdr.flags = ACK_FLAG;
            ack_packet.hdr.ack_num = packet.hdr.seq_num + 1;
            sendto(sockfd, &ack_packet, sizeof(struct sham_header), 0, (struct sockaddr*)client_addr, *addr_len);
            log_event("SND ACK FOR FIN\n");

            struct sham_packet fin_packet;
            memset(&fin_packet, 0, sizeof(fin_packet));
            fin_packet.hdr.flags = FIN_FLAG;
            fin_packet.hdr.seq_num = rand();
            sendto(sockfd, &fin_packet, sizeof(struct sham_header), 0, (struct sockaddr*)client_addr, *addr_len);
            log_event("SND FIN SEQ=%u\n", fin_packet.hdr.seq_num);

            recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr*)client_addr, addr_len);
            if (packet.hdr.flags & ACK_FLAG && packet.hdr.ack_num == fin_packet.hdr.seq_num + 1) {
                 log_event("RCV ACK=%u\n", packet.hdr.ack_num);
            }
            break;
        }

        if (packet.hdr.flags == 0 && (float)rand() / RAND_MAX < loss_rate) {
            log_event("DROP DATA SEQ=%u\n", packet.hdr.seq_num);
            continue;
        }

        log_event("RCV DATA SEQ=%u LEN=%ld\n", packet.hdr.seq_num, n - sizeof(struct sham_header));

        if (packet.hdr.seq_num == next_expected_seq) {
            fwrite(packet.data, 1, n - sizeof(struct sham_header), fp);
            next_expected_seq += (n - sizeof(struct sham_header));
        }

        struct sham_packet ack_packet;
        memset(&ack_packet, 0, sizeof(ack_packet));
        ack_packet.hdr.flags = ACK_FLAG;
        ack_packet.hdr.ack_num = next_expected_seq;
        ack_packet.hdr.window_size = RECEIVER_BUFFER_SIZE;
        sendto(sockfd, &ack_packet, sizeof(struct sham_header), 0, (struct sockaddr*)client_addr, *addr_len);
        log_event("SND ACK=%u WIN=%u\n", ack_packet.hdr.ack_num, ack_packet.hdr.window_size);
    }

    fclose(fp);

    char md5_result[33];
    if (calculate_md5(output_filename, md5_result) == 0) {
        printf("MD5: %s\n", md5_result);
    }
    return 0; // Don't shut down server after file transfer
}

typedef enum {
    STATE_ESTABLISHED,
    STATE_FIN_SENT,
    STATE_CLOSING,
    STATE_TERMINATED
} ConnectionState;

// Returns 0 to continue server, 1 to shut down
int handle_chat_mode(int sockfd, struct sockaddr_in* client_addr, socklen_t* addr_len, float loss_rate) {
    printf("Chat mode started. Type '/quit' to exit.\n");

    ConnectionState state = STATE_ESTABLISHED;
    fd_set read_fds;
    char input_buffer[DATA_CHUNK_SIZE];

    sent_packet_info window[SENDER_WINDOW_PACKETS];
    memset(window, 0, sizeof(window));

    uint32_t my_next_seq_num = rand();
    uint32_t peer_expected_seq_num = 0;
    bool peer_isn_set = false;
    bool peer_fin_rcvd = false;
    bool my_fin_acked = false;

    while (state != STATE_TERMINATED) {
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        if (state == STATE_ESTABLISHED) {
            FD_SET(STDIN_FILENO, &read_fds);
        }

        struct timeval timeout = {0, 100000}; // 100ms
        select(sockfd + 1, &read_fds, NULL, NULL, &timeout);

        if (FD_ISSET(sockfd, &read_fds)) {
            struct sham_packet packet;
            int n = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr*)client_addr, addr_len);
            if (n <= 0) continue;

            if (packet.hdr.flags & ACK_FLAG) {
                // Log moved inside to prevent duplicate logging
                for(int i = 0; i < SENDER_WINDOW_PACKETS; ++i) {
                    if (window[i].data_len > 0 && !window[i].acked) {
                        uint32_t acked_seq = window[i].packet.hdr.seq_num + window[i].data_len;
                        if (acked_seq == packet.hdr.ack_num) {
                            if (window[i].packet.hdr.flags & FIN_FLAG) {
                                log_event("RCV ACK FOR FIN (ACK=%u)\n", packet.hdr.ack_num);
                                my_fin_acked = true;
                                if (peer_fin_rcvd) {
                                    state = STATE_TERMINATED;
                                }
                            } else {
                                log_event("RCV ACK=%u\n", packet.hdr.ack_num);
                            }
                            window[i].acked = 1;
                            window[i].data_len = 0;
                            break;
                        }
                    }
                }
            } else if (packet.hdr.flags & FIN_FLAG) {
                if (!peer_fin_rcvd) {
                    peer_fin_rcvd = true;
                    log_event("RCV FIN SEQ=%u\n", packet.hdr.seq_num);
                    peer_expected_seq_num = packet.hdr.seq_num + 1;

                    struct sham_packet ack_pack;
                    memset(&ack_pack, 0, sizeof(ack_pack));
                    ack_pack.hdr.flags = ACK_FLAG;
                    ack_pack.hdr.ack_num = peer_expected_seq_num;
                    sendto(sockfd, &ack_pack, sizeof(struct sham_header), 0, (struct sockaddr*)client_addr, *addr_len);
                    log_event("SND ACK FOR FIN\n");

                    if (state == STATE_ESTABLISHED) {
                        state = STATE_CLOSING;
                    } else if (state == STATE_FIN_SENT) {
                        if (my_fin_acked) {
                           state = STATE_TERMINATED;
                        }
                    }
                }
            } else { // It's a data packet
                if (!peer_isn_set) {
                    peer_expected_seq_num = packet.hdr.seq_num;
                    peer_isn_set = true;
                }
                if ((float)rand() / RAND_MAX < loss_rate) {
                    log_event("DROP DATA SEQ=%u\n", packet.hdr.seq_num);
                    continue;
                }
                log_event("RCV DATA SEQ=%u\n", packet.hdr.seq_num);
                if (packet.hdr.seq_num >= peer_expected_seq_num) {
                    if (packet.hdr.seq_num == peer_expected_seq_num) {
                        printf("Client: %.*s", (int)(n - sizeof(struct sham_header)), packet.data);
                        peer_expected_seq_num += (n - sizeof(struct sham_header));
                    }
                    struct sham_packet ack_packet;
                    memset(&ack_packet, 0, sizeof(ack_packet));
                    ack_packet.hdr.flags = ACK_FLAG;
                    ack_packet.hdr.ack_num = peer_expected_seq_num;
                    sendto(sockfd, &ack_packet, sizeof(struct sham_header), 0, (struct sockaddr*)client_addr, *addr_len);
                    log_event("SND ACK=%u\n", ack_packet.hdr.ack_num);
                }
            }
        }

        if (state == STATE_ESTABLISHED && FD_ISSET(STDIN_FILENO, &read_fds)) {
            int n = read(STDIN_FILENO, input_buffer, sizeof(input_buffer) - 1);
            if (n > 0) {
                input_buffer[n] = '\0';
                if (strncmp(input_buffer, "/quit\n", 6) == 0) {
                    state = STATE_FIN_SENT;
                } else {
                    int win_idx = -1;
                    for(int i=0; i<SENDER_WINDOW_PACKETS; ++i) if(window[i].data_len == 0) {win_idx = i; break;}
                    if(win_idx != -1) {
                         window[win_idx].packet.hdr.seq_num = my_next_seq_num;
                         window[win_idx].packet.hdr.flags = 0;
                         memcpy(window[win_idx].packet.data, input_buffer, n);
                         window[win_idx].data_len = n;
                         window[win_idx].acked = 0;
                         window[win_idx].send_time = get_current_time();
                         sendto(sockfd, &window[win_idx].packet, sizeof(struct sham_header) + n, 0, (struct sockaddr*)client_addr, *addr_len);
                         log_event("SND DATA SEQ=%u LEN=%d\n", my_next_seq_num, n);
                         my_next_seq_num += n;
                    }
                }
            }
        }

        if (state == STATE_FIN_SENT || state == STATE_CLOSING) {
            bool my_fin_sent = false;
            for(int i=0; i<SENDER_WINDOW_PACKETS; ++i) if(window[i].packet.hdr.flags & FIN_FLAG) my_fin_sent = true;

            if (!my_fin_sent) {
                int win_idx = -1;
                for(int i=0; i<SENDER_WINDOW_PACKETS; ++i) if(window[i].data_len == 0) {win_idx = i; break;}
                if(win_idx != -1) {
                    window[win_idx].packet.hdr.seq_num = my_next_seq_num;
                    window[win_idx].packet.hdr.flags = FIN_FLAG;
                    window[win_idx].data_len = 1;
                    window[win_idx].acked = 0;
                    window[win_idx].send_time = get_current_time();
                    sendto(sockfd, &window[win_idx].packet, sizeof(struct sham_header), 0, (struct sockaddr*)client_addr, *addr_len);
                    log_event("SND FIN SEQ=%u\n", my_next_seq_num);
                    my_next_seq_num++;
                }
            }
        }

        struct timeval now = get_current_time();
        for (int i = 0; i < SENDER_WINDOW_PACKETS; i++) {
            if (window[i].data_len > 0 && !window[i].acked) {
                if (time_diff_ms(now, window[i].send_time) > RTO_MS) {
                    log_event("TIMEOUT SEQ=%u\n", window[i].packet.hdr.seq_num);
                    int len = (window[i].packet.hdr.flags & FIN_FLAG) ? 0 : window[i].data_len;
                    const char* type = (window[i].packet.hdr.flags & FIN_FLAG) ? "FIN" : "DATA";
                    sendto(sockfd, &window[i].packet, sizeof(struct sham_header) + len, 0, (struct sockaddr*)client_addr, *addr_len);
                    log_event("RETX %s SEQ=%u\n", type, window[i].packet.hdr.seq_num);
                    window[i].send_time = get_current_time();
                }
            }
        }
    }
    printf("Connection terminated.\n");
    return 1; // **CHANGE**: Signal main loop to shut down
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port> [--chat] [loss_rate]\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int chat_mode = 0;
    float loss_rate = 0.0;

    if (argc > 2) {
        if (strcmp(argv[2], "--chat") == 0) {
            chat_mode = 1;
            if (argc > 3) loss_rate = atof(argv[3]);
        } else {
            loss_rate = atof(argv[2]);
        }
    }
     if (argc > 3 && strcmp(argv[2], "--chat") == 0) {
        loss_rate = atof(argv[3]);
    }

    setup_logging("server_log.txt");
    srand(time(NULL));

    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);

    while (1) {
        struct sham_packet packet;
        int n = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr*)&client_addr, &addr_len);

        if (n > 0 && (packet.hdr.flags & SYN_FLAG)) {
            log_event("RCV SYN SEQ=%u\n", packet.hdr.seq_num);

            uint32_t client_isn = packet.hdr.seq_num;
            uint32_t server_isn = rand();

            struct sham_packet syn_ack_packet;
            memset(&syn_ack_packet, 0, sizeof(syn_ack_packet));
            syn_ack_packet.hdr.flags = SYN_FLAG | ACK_FLAG;
            syn_ack_packet.hdr.seq_num = server_isn;
            syn_ack_packet.hdr.ack_num = client_isn + 1;

            sendto(sockfd, &syn_ack_packet, sizeof(struct sham_header), 0, (struct sockaddr*)&client_addr, addr_len);
            log_event("SND SYN-ACK SEQ=%u ACK=%u\n", server_isn, client_isn + 1);

            n = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr*)&client_addr, &addr_len);
            if (n > 0 && (packet.hdr.flags & ACK_FLAG) && packet.hdr.ack_num == server_isn + 1) {
                log_event("RCV ACK FOR SYN\n");
                printf("Connection established with %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                int shutdown_signal = 0;
                if (chat_mode) {
                    // **CHANGE**: Check the return value from handle_chat_mode
                    shutdown_signal = handle_chat_mode(sockfd, &client_addr, &addr_len, loss_rate);
                } else {
                    shutdown_signal = handle_file_transfer(sockfd, &client_addr, &addr_len, loss_rate);
                }

                if (shutdown_signal) {
                    printf("Chat session ended. Server shutting down.\n");
                    break; // **CHANGE**: Exit the while(1) loop
                }

                printf("Connection closed. Waiting for new connection...\n");
            }
        }
    }

    close(sockfd);
    if (log_file) fclose(log_file);
    return 0;
}
/* ############## LLM Generated Code Ends ################ */