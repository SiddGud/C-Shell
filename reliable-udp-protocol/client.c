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

struct timeval get_current_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv;
}

long time_diff_ms(struct timeval end, struct timeval start) {
    return (end.tv_sec - start.tv_sec) * 1000L + (end.tv_usec - start.tv_usec) / 1000L;
}

void handle_file_transfer(int sockfd, struct sockaddr_in* server_addr, const char* input_file, const char* output_file, float loss_rate) {
     FILE* fp = fopen(input_file, "rb");
    if (!fp) {
        perror("Failed to open input file");
        return;
    }

    struct sham_packet name_packet;
    memset(&name_packet, 0, sizeof(name_packet));
    name_packet.hdr.seq_num = 1;
    strncpy(name_packet.data, output_file, strlen(output_file));
    sendto(sockfd, &name_packet, sizeof(struct sham_header) + strlen(output_file), 0, (struct sockaddr*)server_addr, sizeof(*server_addr));
    log_event("SND DATA SEQ=%u LEN=%ld (Filename: %s)\n", name_packet.hdr.seq_num, strlen(output_file), output_file);

    recvfrom(sockfd, &name_packet, sizeof(name_packet), 0, NULL, NULL);
    log_event("RCV ACK=%u\n", name_packet.hdr.ack_num);

    sent_packet_info window[SENDER_WINDOW_PACKETS];
    memset(window, 0, sizeof(window));

    uint32_t base = name_packet.hdr.ack_num;
    uint32_t next_seq_num = base;
    int file_done = 0;
    uint32_t receiver_window = 65535;

    fd_set read_fds;
    struct timeval timeout;

    while (!file_done || base < next_seq_num) {
        int unacked_packets = 0;
        for(int i = 0; i < SENDER_WINDOW_PACKETS; i++) {
            if(window[i].data_len > 0 && !window[i].acked) unacked_packets++;
        }

        while (unacked_packets < SENDER_WINDOW_PACKETS && !file_done) {
            char buffer[DATA_CHUNK_SIZE];
            int bytes_read = fread(buffer, 1, DATA_CHUNK_SIZE, fp);

            if (bytes_read > 0) {
                 int win_idx = -1;
                 for(int i=0; i<SENDER_WINDOW_PACKETS; ++i) if(window[i].data_len == 0) { win_idx = i; break;}
                 if(win_idx == -1) break;

                window[win_idx].packet.hdr.seq_num = next_seq_num;
                window[win_idx].packet.hdr.flags = 0;
                memcpy(window[win_idx].packet.data, buffer, bytes_read);
                window[win_idx].data_len = bytes_read;
                window[win_idx].acked = 0;
                window[win_idx].send_time = get_current_time();

                sendto(sockfd, &window[win_idx].packet, sizeof(struct sham_header) + bytes_read, 0, (struct sockaddr*)server_addr, sizeof(*server_addr));
                log_event("SND DATA SEQ=%u LEN=%d\n", next_seq_num, bytes_read);

                next_seq_num += bytes_read;
                unacked_packets++;
            } else {
                if (ferror(fp)) perror("fread");
                file_done = 1;
                break;
            }
        }

        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100 * 1000;

        int activity = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);

        if (activity > 0) {
            struct sham_packet ack_packet;
            recvfrom(sockfd, &ack_packet, sizeof(ack_packet), 0, NULL, NULL);
             if ((float)rand() / RAND_MAX < loss_rate) {
                log_event("DROP ACK=%u\n", ack_packet.hdr.ack_num);
             } else if (ack_packet.hdr.flags & ACK_FLAG) {
                log_event("RCV ACK=%u\n", ack_packet.hdr.ack_num);
                if(ack_packet.hdr.window_size != receiver_window) {
                    receiver_window = ack_packet.hdr.window_size;
                    log_event("FLOW WIN UPDATE=%u\n", receiver_window);
                }
                if(ack_packet.hdr.ack_num > base) {
                    base = ack_packet.hdr.ack_num;
                    for(int i = 0; i < SENDER_WINDOW_PACKETS; ++i) {
                        if (window[i].data_len > 0 && !window[i].acked) {
                            if (window[i].packet.hdr.seq_num < base) {
                                window[i].acked = 1;
                                window[i].data_len = 0;
                            }
                        }
                    }
                }
            }
        }

        struct timeval now = get_current_time();
        for (int i = 0; i < SENDER_WINDOW_PACKETS; i++) {
            if (window[i].data_len > 0 && !window[i].acked) {
                if (time_diff_ms(now, window[i].send_time) > RTO_MS) {
                    log_event("TIMEOUT SEQ=%u\n", window[i].packet.hdr.seq_num);
                    sendto(sockfd, &window[i].packet, sizeof(struct sham_header) + window[i].data_len, 0, (struct sockaddr*)server_addr, sizeof(*server_addr));
                    log_event("RETX DATA SEQ=%u LEN=%d\n", window[i].packet.hdr.seq_num, window[i].data_len);
                    window[i].send_time = get_current_time();
                }
            }
        }
    }

    struct sham_packet fin_packet;
    memset(&fin_packet, 0, sizeof(fin_packet));
    fin_packet.hdr.flags = FIN_FLAG;
    fin_packet.hdr.seq_num = next_seq_num;
    sendto(sockfd, &fin_packet, sizeof(struct sham_header), 0, (struct sockaddr*)server_addr, sizeof(*server_addr));
    log_event("SND FIN SEQ=%u\n", fin_packet.hdr.seq_num);

    recvfrom(sockfd, &fin_packet, sizeof(fin_packet), 0, NULL, NULL);
    log_event("RCV ACK FOR FIN\n");
    recvfrom(sockfd, &fin_packet, sizeof(fin_packet), 0, NULL, NULL);
    log_event("RCV FIN SEQ=%u\n", fin_packet.hdr.seq_num);

    struct sham_packet final_ack;
    memset(&final_ack, 0, sizeof(final_ack));
    final_ack.hdr.flags = ACK_FLAG;
    final_ack.hdr.ack_num = fin_packet.hdr.seq_num + 1;
    sendto(sockfd, &final_ack, sizeof(struct sham_header), 0, (struct sockaddr*)server_addr, sizeof(*server_addr));
    log_event("SND ACK=%u\n", final_ack.hdr.ack_num);

    fclose(fp);
    printf("File transfer complete.\n");
}

typedef enum {
    STATE_ESTABLISHED,
    STATE_FIN_SENT,
    STATE_CLOSING,
    STATE_TERMINATED
} ConnectionState;

void handle_chat_mode(int sockfd, struct sockaddr_in* server_addr, float loss_rate) {
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
            int n = recvfrom(sockfd, &packet, sizeof(packet), 0, NULL, NULL);
            if (n <= 0) continue;

            if (packet.hdr.flags & ACK_FLAG) {
                log_event("RCV ACK=%u\n", packet.hdr.ack_num);
                for(int i = 0; i < SENDER_WINDOW_PACKETS; ++i) {
                     if (window[i].data_len > 0 && !window[i].acked) {
                        uint32_t acked_seq = window[i].packet.hdr.seq_num + window[i].data_len;
                        if (acked_seq == packet.hdr.ack_num) {
                            window[i].acked = 1;
                             if (window[i].packet.hdr.flags & FIN_FLAG) {
                                log_event("RCV ACK FOR FIN\n");
                                my_fin_acked = true;
                                if (peer_fin_rcvd) {
                                    state = STATE_TERMINATED;
                                }
                            }
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
                    sendto(sockfd, &ack_pack, sizeof(struct sham_header), 0, (struct sockaddr*)server_addr, sizeof(*server_addr));
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
                        printf("Server: %.*s", (int)(n - sizeof(struct sham_header)), packet.data);
                        peer_expected_seq_num += (n - sizeof(struct sham_header));
                    }
                    struct sham_packet ack_packet;
                    memset(&ack_packet, 0, sizeof(ack_packet));
                    ack_packet.hdr.flags = ACK_FLAG;
                    ack_packet.hdr.ack_num = peer_expected_seq_num;
                    sendto(sockfd, &ack_packet, sizeof(struct sham_header), 0, (struct sockaddr*)server_addr, sizeof(*server_addr));
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
                         sendto(sockfd, &window[win_idx].packet, sizeof(struct sham_header) + n, 0, (struct sockaddr*)server_addr, sizeof(*server_addr));
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
                    sendto(sockfd, &window[win_idx].packet, sizeof(struct sham_header), 0, (struct sockaddr*)server_addr, sizeof(*server_addr));
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
                    sendto(sockfd, &window[i].packet, sizeof(struct sham_header) + len, 0, (struct sockaddr*)server_addr, sizeof(*server_addr));
                    log_event("RETX %s SEQ=%u\n", type, window[i].packet.hdr.seq_num);
                    window[i].send_time = get_current_time();
                }
            }
        }
    }
    printf("Connection terminated.\n");
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  File Transfer: %s <server_ip> <server_port> <input_file> <output_file_name> [loss_rate]\n", argv[0]);
        fprintf(stderr, "  Chat Mode:     %s <server_ip> <server_port> --chat [loss_rate]\n", argv[0]);
        return 1;
    }

    char* server_ip = argv[1];
    int port = atoi(argv[2]);
    int chat_mode = 0;
    float loss_rate = 0.0;
    char *input_file = NULL, *output_file = NULL;

    if (argc > 3 && strcmp(argv[3], "--chat") == 0) {
        chat_mode = 1;
        if (argc > 4) loss_rate = atof(argv[4]);
    } else {
        if (argc < 5) {
            fprintf(stderr, "Missing file arguments for file transfer mode.\n");
            return 1;
        }
        input_file = argv[3];
        output_file = argv[4];
        if (argc > 5) loss_rate = atof(argv[5]);
    }

    setup_logging("client_log.txt");
    srand(time(NULL));

    int sockfd;
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_aton(server_ip, &server_addr.sin_addr) == 0) {
        fprintf(stderr, "Invalid address\n");
        exit(EXIT_FAILURE);
    }

    struct sham_packet syn_packet, response_packet;
    uint32_t client_isn = rand();

    memset(&syn_packet, 0, sizeof(syn_packet));
    syn_packet.hdr.flags = SYN_FLAG;
    syn_packet.hdr.seq_num = client_isn;
    sendto(sockfd, &syn_packet, sizeof(struct sham_header), 0, (const struct sockaddr*)&server_addr, sizeof(server_addr));
    log_event("SND SYN SEQ=%u\n", client_isn);

    recvfrom(sockfd, &response_packet, sizeof(response_packet), 0, NULL, NULL);
    if ((response_packet.hdr.flags & (SYN_FLAG | ACK_FLAG)) && response_packet.hdr.ack_num == client_isn + 1) {
        log_event("RCV SYN-ACK SEQ=%u ACK=%u\n", response_packet.hdr.seq_num, response_packet.hdr.ack_num);

        uint32_t server_isn = response_packet.hdr.seq_num;
        struct sham_packet ack_packet;
        memset(&ack_packet, 0, sizeof(ack_packet));
        ack_packet.hdr.flags = ACK_FLAG;
        ack_packet.hdr.ack_num = server_isn + 1;
        sendto(sockfd, &ack_packet, sizeof(struct sham_header), 0, (const struct sockaddr*)&server_addr, sizeof(server_addr));
        log_event("SND ACK FOR SYN\n");
        printf("Connection established.\n");

        if (chat_mode) {
            handle_chat_mode(sockfd, &server_addr, loss_rate);
        } else {
            handle_file_transfer(sockfd, &server_addr, input_file, output_file, loss_rate);
        }
    } else {
        fprintf(stderr, "Handshake failed.\n");
    }

    close(sockfd);
    if(log_file) fclose(log_file);
    return 0;
}
/* ############## LLM Generated Code Ends ################ */