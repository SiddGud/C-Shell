#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcap.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <net/if_arp.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#define MAX_PACKETS 10000
#define SNAP_LEN 65535

// Stored packet structure
typedef struct {
    int id;
    struct timeval timestamp;
    int length;
    unsigned char *data;
} stored_packet_t;

// Global variables
stored_packet_t packets[MAX_PACKETS];
int packet_count = 0;
pcap_t *global_handle = NULL;
volatile sig_atomic_t stop_sniffing = 0;

// Function prototypes
void print_banner();
void list_interfaces();
void main_menu(char *interface_name);
void start_sniffing_all(char *interface_name);
void start_sniffing_filtered(char *interface_name);
void inspect_last_session();
void packet_handler(unsigned char *user_data, const struct pcap_pkthdr *pkthdr, const unsigned char *packet);
void process_packet(int packet_id, const struct pcap_pkthdr *pkthdr, const unsigned char *packet, int store_packet);
void print_hex_dump(const unsigned char *data, int length, int max_bytes);
void print_full_hex_dump(const unsigned char *data, int length);
void free_stored_packets();
void signal_handler(int signum);
const char* get_port_name(int port);
void print_ethernet_header(const unsigned char *packet);
void print_ipv4_header(const unsigned char *packet, int *proto);
void print_ipv6_header(const unsigned char *packet, int *proto);
void print_arp_header(const unsigned char *packet);
void print_tcp_header(const unsigned char *packet, int is_ipv6, int *payload_offset, int *payload_len, int total_len);
void print_udp_header(const unsigned char *packet, int is_ipv6, int *payload_offset, int *payload_len, int total_len);
void print_payload(const unsigned char *packet, int offset, int length, int src_port, int dst_port);

int main() {
    signal(SIGINT, signal_handler);
    
    print_banner();
    list_interfaces();
    
    return 0;
}

void print_banner() {
    printf("\n[C-Shark] The Command-Line Packet Predator\n");
    printf("==============================================\n");
}

void list_interfaces() {
    pcap_if_t *alldevs;
    pcap_if_t *device;
    char errbuf[PCAP_ERRBUF_SIZE];
    int i = 0;
    int choice;
    
    printf("[C-Shark] Searching for available interfaces... Found!\n\n");
    
    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        fprintf(stderr, "Error finding devices: %s\n", errbuf);
        exit(1);
    }
    
    for (device = alldevs; device != NULL; device = device->next) {
        i++;
        printf("%d. %s", i, device->name);
        if (device->description) {
            printf(" (%s)", device->description);
        }
        printf("\n");
    }
    
    printf("\nSelect an interface to sniff (1-%d): ", i);
    scanf("%d", &choice);
    getchar(); // consume newline
    
    if (choice < 1 || choice > i) {
        printf("Invalid choice!\n");
        pcap_freealldevs(alldevs);
        exit(1);
    }
    
    // Get selected interface
    device = alldevs;
    for (int j = 1; j < choice; j++) {
        device = device->next;
    }
    
    char interface_name[256];
    strcpy(interface_name, device->name);
    
    pcap_freealldevs(alldevs);
    
    main_menu(interface_name);
}

void main_menu(char *interface_name) {
    int choice;
    
    while (1) {
        printf("\n[C-Shark] Interface '%s' selected. What's next?\n\n", interface_name);
        printf("1. Start Sniffing (All Packets)\n");
        printf("2. Start Sniffing (With Filters)\n");
        printf("3. Inspect Last Session\n");
        printf("4. Exit C-Shark\n\n");
        printf("Enter your choice: ");
        
        if (scanf("%d", &choice) != 1) {
            // Handle Ctrl+D
            printf("\n[C-Shark] Exiting gracefully...\n");
            free_stored_packets();
            exit(0);
        }
        getchar(); // consume newline
        
        switch (choice) {
            case 1:
                start_sniffing_all(interface_name);
                break;
            case 2:
                start_sniffing_filtered(interface_name);
                break;
            case 3:
                inspect_last_session();
                break;
            case 4:
                printf("[C-Shark] Exiting gracefully...\n");
                free_stored_packets();
                exit(0);
            default:
                printf("Invalid choice!\n");
        }
    }
}

void signal_handler(int signum) {
    if (signum == SIGINT) {
        stop_sniffing = 1;
        if (global_handle) {
            pcap_breakloop(global_handle);
        }
    }
}

void start_sniffing_all(char *interface_name) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle;
    
    // Free previous session
    free_stored_packets();
    
    handle = pcap_open_live(interface_name, SNAP_LEN, 1, 1000, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open device %s: %s\n", interface_name, errbuf);
        return;
    }
    
    global_handle = handle;
    stop_sniffing = 0;
    
    printf("\n[C-Shark] Starting packet capture on '%s'...\n", interface_name);
    printf("[C-Shark] Press Ctrl+C to stop and return to menu.\n\n");
    
    pcap_loop(handle, -1, packet_handler, NULL);
    
    pcap_close(handle);
    global_handle = NULL;
    
    if (stop_sniffing) {
        printf("\n\n[C-Shark] Capture stopped. Returning to menu...\n");
    }
}

void start_sniffing_filtered(char *interface_name) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle;
    struct bpf_program fp;
    bpf_u_int32 net, mask;
    char filter_exp[256];
    int filter_choice;
    
    printf("\nSelect a filter:\n");
    printf("1. HTTP\n");
    printf("2. HTTPS\n");
    printf("3. DNS\n");
    printf("4. ARP\n");
    printf("5. TCP\n");
    printf("6. UDP\n");
    printf("Enter your choice: ");
    scanf("%d", &filter_choice);
    getchar();
    
    switch (filter_choice) {
        case 1:
            strcpy(filter_exp, "tcp port 80");
            break;
        case 2:
            strcpy(filter_exp, "tcp port 443");
            break;
        case 3:
            strcpy(filter_exp, "udp port 53");
            break;
        case 4:
            strcpy(filter_exp, "arp");
            break;
        case 5:
            strcpy(filter_exp, "tcp");
            break;
        case 6:
            strcpy(filter_exp, "udp");
            break;
        default:
            printf("Invalid choice!\n");
            return;
    }
    
    // Free previous session
    free_stored_packets();
    
    if (pcap_lookupnet(interface_name, &net, &mask, errbuf) == -1) {
        fprintf(stderr, "Can't get netmask for device %s\n", interface_name);
        net = 0;
        mask = 0;
    }
    
    handle = pcap_open_live(interface_name, SNAP_LEN, 1, 1000, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open device %s: %s\n", interface_name, errbuf);
        return;
    }
    
    if (pcap_compile(handle, &fp, filter_exp, 0, net) == -1) {
        fprintf(stderr, "Couldn't parse filter %s: %s\n", filter_exp, pcap_geterr(handle));
        pcap_close(handle);
        return;
    }
    
    if (pcap_setfilter(handle, &fp) == -1) {
        fprintf(stderr, "Couldn't install filter %s: %s\n", filter_exp, pcap_geterr(handle));
        pcap_freecode(&fp);
        pcap_close(handle);
        return;
    }
    
    global_handle = handle;
    stop_sniffing = 0;
    
    printf("\n[C-Shark] Starting filtered packet capture on '%s' (Filter: %s)...\n", interface_name, filter_exp);
    printf("[C-Shark] Press Ctrl+C to stop and return to menu.\n\n");
    
    pcap_loop(handle, -1, packet_handler, NULL);
    
    pcap_freecode(&fp);
    pcap_close(handle);
    global_handle = NULL;
    
    if (stop_sniffing) {
        printf("\n\n[C-Shark] Capture stopped. Returning to menu...\n");
    }
}

void packet_handler(unsigned char *user_data, const struct pcap_pkthdr *pkthdr, const unsigned char *packet) {
    (void)user_data; // Unused parameter
    static int packet_id = 0;
    packet_id++;
    
    process_packet(packet_id, pkthdr, packet, 1);
}

void process_packet(int packet_id, const struct pcap_pkthdr *pkthdr, const unsigned char *packet, int store_packet) {
    struct ether_header *eth_header;
    int ether_type;
    int protocol = -1;
    int payload_offset = 0;
    int payload_len = 0;
    int src_port = 0, dst_port = 0;
    
    printf("-----------------------------------------\n");
    printf("Packet #%d | Timestamp: %ld.%06ld | Length: %d bytes\n",
           packet_id, pkthdr->ts.tv_sec, pkthdr->ts.tv_usec, pkthdr->len);
    
    // Store packet if requested
    if (store_packet && packet_count < MAX_PACKETS) {
        packets[packet_count].id = packet_id;
        packets[packet_count].timestamp = pkthdr->ts;
        packets[packet_count].length = pkthdr->len;
        packets[packet_count].data = (unsigned char *)malloc(pkthdr->len);
        if (packets[packet_count].data) {
            memcpy(packets[packet_count].data, packet, pkthdr->len);
            packet_count++;
        }
    }
    
    // Layer 2: Ethernet
    print_ethernet_header(packet);
    
    eth_header = (struct ether_header *)packet;
    ether_type = ntohs(eth_header->ether_type);
    
    // Layer 3: Network
    if (ether_type == ETHERTYPE_IP) {
        print_ipv4_header(packet, &protocol);
        
        // Layer 4: Transport
        if (protocol == IPPROTO_TCP) {
            print_tcp_header(packet, 0, &payload_offset, &payload_len, pkthdr->len);
            struct tcphdr *tcp = (struct tcphdr *)(packet + sizeof(struct ether_header) + (((struct ip *)(packet + sizeof(struct ether_header)))->ip_hl * 4));
            src_port = ntohs(tcp->th_sport);
            dst_port = ntohs(tcp->th_dport);
            
            // Layer 7: Payload
            if (payload_len > 0) {
                print_payload(packet, payload_offset, payload_len, src_port, dst_port);
            }
        } else if (protocol == IPPROTO_UDP) {
            print_udp_header(packet, 0, &payload_offset, &payload_len, pkthdr->len);
            struct udphdr *udp = (struct udphdr *)(packet + sizeof(struct ether_header) + (((struct ip *)(packet + sizeof(struct ether_header)))->ip_hl * 4));
            src_port = ntohs(udp->uh_sport);
            dst_port = ntohs(udp->uh_dport);
            
            // Layer 7: Payload
            if (payload_len > 0) {
                print_payload(packet, payload_offset, payload_len, src_port, dst_port);
            }
        }
    } else if (ether_type == ETHERTYPE_IPV6) {
        print_ipv6_header(packet, &protocol);
        
        // Layer 4: Transport
        if (protocol == IPPROTO_TCP) {
            print_tcp_header(packet, 1, &payload_offset, &payload_len, pkthdr->len);
            struct tcphdr *tcp = (struct tcphdr *)(packet + sizeof(struct ether_header) + 40);
            src_port = ntohs(tcp->th_sport);
            dst_port = ntohs(tcp->th_dport);
            
            // Layer 7: Payload
            if (payload_len > 0) {
                print_payload(packet, payload_offset, payload_len, src_port, dst_port);
            }
        } else if (protocol == IPPROTO_UDP) {
            print_udp_header(packet, 1, &payload_offset, &payload_len, pkthdr->len);
            struct udphdr *udp = (struct udphdr *)(packet + sizeof(struct ether_header) + 40);
            src_port = ntohs(udp->uh_sport);
            dst_port = ntohs(udp->uh_dport);
            
            // Layer 7: Payload
            if (payload_len > 0) {
                print_payload(packet, payload_offset, payload_len, src_port, dst_port);
            }
        }
    } else if (ether_type == ETHERTYPE_ARP) {
        print_arp_header(packet);
    }
}

void print_ethernet_header(const unsigned char *packet) {
    struct ether_header *eth_header = (struct ether_header *)packet;
    int ether_type = ntohs(eth_header->ether_type);
    
    printf("L2 (Ethernet): Dst MAC: %02X:%02X:%02X:%02X:%02X:%02X | Src MAC: %02X:%02X:%02X:%02X:%02X:%02X | \n",
           eth_header->ether_dhost[0], eth_header->ether_dhost[1], eth_header->ether_dhost[2],
           eth_header->ether_dhost[3], eth_header->ether_dhost[4], eth_header->ether_dhost[5],
           eth_header->ether_shost[0], eth_header->ether_shost[1], eth_header->ether_shost[2],
           eth_header->ether_shost[3], eth_header->ether_shost[4], eth_header->ether_shost[5]);
    
    if (ether_type == ETHERTYPE_IP) {
        printf("EtherType: IPv4 (0x%04X)\n", ether_type);
    } else if (ether_type == ETHERTYPE_IPV6) {
        printf("EtherType: IPv6 (0x%04X)\n", ether_type);
    } else if (ether_type == ETHERTYPE_ARP) {
        printf("EtherType: ARP (0x%04X)\n", ether_type);
    } else {
        printf("EtherType: Unknown (0x%04X)\n", ether_type);
    }
}

void print_ipv4_header(const unsigned char *packet, int *proto) {
    struct ip *ip_header = (struct ip *)(packet + sizeof(struct ether_header));
    char src_ip[INET_ADDRSTRLEN];
    char dst_ip[INET_ADDRSTRLEN];
    
    inet_ntop(AF_INET, &(ip_header->ip_src), src_ip, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(ip_header->ip_dst), dst_ip, INET_ADDRSTRLEN);
    
    *proto = ip_header->ip_p;
    
    printf("L3 (IPv4): Src IP: %s | Dst IP: %s | Protocol: ", src_ip, dst_ip);
    
    if (ip_header->ip_p == IPPROTO_TCP) {
        printf("TCP (6) | \n");
    } else if (ip_header->ip_p == IPPROTO_UDP) {
        printf("UDP (17) | \n");
    } else if (ip_header->ip_p == IPPROTO_ICMP) {
        printf("ICMP (1) | \n");
    } else {
        printf("Unknown (%d) | \n", ip_header->ip_p);
    }
    
    printf("TTL: %d\n", ip_header->ip_ttl);
    printf("ID: 0x%04X | Total Length: %d | Header Length: %d bytes\n",
           ntohs(ip_header->ip_id), ntohs(ip_header->ip_len), ip_header->ip_hl * 4);
}

void print_ipv6_header(const unsigned char *packet, int *proto) {
    struct ip6_hdr *ip6_header = (struct ip6_hdr *)(packet + sizeof(struct ether_header));
    char src_ip[INET6_ADDRSTRLEN];
    char dst_ip[INET6_ADDRSTRLEN];
    
    inet_ntop(AF_INET6, &(ip6_header->ip6_src), src_ip, INET6_ADDRSTRLEN);
    inet_ntop(AF_INET6, &(ip6_header->ip6_dst), dst_ip, INET6_ADDRSTRLEN);
    
    *proto = ip6_header->ip6_nxt;
    
    printf("L3 (IPv6): Src IP: %s | Dst IP: %s | \n", src_ip, dst_ip);
    
    printf("Next Header: ");
    if (ip6_header->ip6_nxt == IPPROTO_TCP) {
        printf("TCP (6) | ");
    } else if (ip6_header->ip6_nxt == IPPROTO_UDP) {
        printf("UDP (17) | ");
    } else if (ip6_header->ip6_nxt == IPPROTO_ICMPV6) {
        printf("ICMPv6 (58) | ");
    } else {
        printf("Unknown (%d) | ", ip6_header->ip6_nxt);
    }
    
    printf("Hop Limit: %d | \n", ip6_header->ip6_hlim);
    
    int traffic_class = (ntohl(ip6_header->ip6_flow) >> 20) & 0xFF;
    int flow_label = ntohl(ip6_header->ip6_flow) & 0xFFFFF;
    
    printf("Traffic Class: %d | Flow Label: 0x%05X | Payload Length: %d\n",
           traffic_class, flow_label, ntohs(ip6_header->ip6_plen));
}

void print_arp_header(const unsigned char *packet) {
    struct ether_arp *arp_header = (struct ether_arp *)(packet + sizeof(struct ether_header));
    char sender_ip[INET_ADDRSTRLEN];
    char target_ip[INET_ADDRSTRLEN];
    
    memcpy(&sender_ip, arp_header->arp_spa, 4);
    memcpy(&target_ip, arp_header->arp_tpa, 4);
    
    printf("\nL3 (ARP): Operation: ");
    int op = ntohs(arp_header->arp_op);
    if (op == ARPOP_REQUEST) {
        printf("Request (1) | ");
    } else if (op == ARPOP_REPLY) {
        printf("Reply (2) | ");
    } else {
        printf("Unknown (%d) | ", op);
    }
    
    printf("Sender IP: %d.%d.%d.%d | Target IP: \n%d.%d.%d.%d\n",
           arp_header->arp_spa[0], arp_header->arp_spa[1], 
           arp_header->arp_spa[2], arp_header->arp_spa[3],
           arp_header->arp_tpa[0], arp_header->arp_tpa[1], 
           arp_header->arp_tpa[2], arp_header->arp_tpa[3]);
    
    printf("Sender MAC: %02X:%02X:%02X:%02X:%02X:%02X | Target MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           arp_header->arp_sha[0], arp_header->arp_sha[1], arp_header->arp_sha[2],
           arp_header->arp_sha[3], arp_header->arp_sha[4], arp_header->arp_sha[5],
           arp_header->arp_tha[0], arp_header->arp_tha[1], arp_header->arp_tha[2],
           arp_header->arp_tha[3], arp_header->arp_tha[4], arp_header->arp_tha[5]);
    
    printf("HW Type: %d | Proto Type: 0x%04X | HW Len: %d | Proto Len: %d\n",
           ntohs(arp_header->arp_hrd), ntohs(arp_header->arp_pro),
           arp_header->arp_hln, arp_header->arp_pln);
}

void print_tcp_header(const unsigned char *packet, int is_ipv6, int *payload_offset, int *payload_len, int total_len) {
    int ip_header_len;
    struct tcphdr *tcp;
    
    if (is_ipv6) {
        ip_header_len = 40;
        tcp = (struct tcphdr *)(packet + sizeof(struct ether_header) + ip_header_len);
    } else {
        struct ip *ip_header = (struct ip *)(packet + sizeof(struct ether_header));
        ip_header_len = ip_header->ip_hl * 4;
        tcp = (struct tcphdr *)(packet + sizeof(struct ether_header) + ip_header_len);
    }
    
    int src_port = ntohs(tcp->th_sport);
    int dst_port = ntohs(tcp->th_dport);
    
    printf("L4 (TCP): Src Port: %d", src_port);
    const char *src_name = get_port_name(src_port);
    if (src_name) printf(" (%s)", src_name);
    
    printf(" | Dst Port: %d", dst_port);
    const char *dst_name = get_port_name(dst_port);
    if (dst_name) printf(" (%s)", dst_name);
    
    printf(" | Seq: %u | Ack: %u | \n", ntohl(tcp->th_seq), ntohl(tcp->th_ack));
    
    printf("Flags: [");
    int flag_count = 0;
    if (tcp->th_flags & TH_SYN) { if (flag_count++) printf(","); printf("SYN"); }
    if (tcp->th_flags & TH_ACK) { if (flag_count++) printf(","); printf("ACK"); }
    if (tcp->th_flags & TH_FIN) { if (flag_count++) printf(","); printf("FIN"); }
    if (tcp->th_flags & TH_RST) { if (flag_count++) printf(","); printf("RST"); }
    if (tcp->th_flags & TH_PUSH) { if (flag_count++) printf(","); printf("PSH"); }
    if (tcp->th_flags & TH_URG) { if (flag_count++) printf(","); printf("URG"); }
    printf("]\n");
    
    int tcp_header_len = tcp->th_off * 4;
    
    printf("Window: %d | Checksum: 0x%04X | Header Length: %d bytes\n",
           ntohs(tcp->th_win), ntohs(tcp->th_sum), tcp_header_len);
    
    *payload_offset = sizeof(struct ether_header) + ip_header_len + tcp_header_len;
    *payload_len = total_len - *payload_offset;
}

void print_udp_header(const unsigned char *packet, int is_ipv6, int *payload_offset, int *payload_len, int total_len) {
    (void)total_len; // Unused parameter
    int ip_header_len;
    struct udphdr *udp;
    
    if (is_ipv6) {
        ip_header_len = 40;
        udp = (struct udphdr *)(packet + sizeof(struct ether_header) + ip_header_len);
    } else {
        struct ip *ip_header = (struct ip *)(packet + sizeof(struct ether_header));
        ip_header_len = ip_header->ip_hl * 4;
        udp = (struct udphdr *)(packet + sizeof(struct ether_header) + ip_header_len);
    }
    
    int src_port = ntohs(udp->uh_sport);
    int dst_port = ntohs(udp->uh_dport);
    
    printf("L4 (UDP): Src Port: %d", src_port);
    const char *src_name = get_port_name(src_port);
    if (src_name) printf(" (%s)", src_name);
    
    printf(" | Dst Port: %d", dst_port);
    const char *dst_name = get_port_name(dst_port);
    if (dst_name) printf(" (%s)", dst_name);
    
    printf(" | Length: %d | Checksum: 0x%04X\n",
           ntohs(udp->uh_ulen), ntohs(udp->uh_sum));
    
    *payload_offset = sizeof(struct ether_header) + ip_header_len + 8;
    *payload_len = ntohs(udp->uh_ulen) - 8;
}

void print_payload(const unsigned char *packet, int offset, int length, int src_port, int dst_port) {
    printf("L7 (Payload): Identified as ");
    
    if (src_port == 80 || dst_port == 80) {
        printf("HTTP on port 80");
    } else if (src_port == 443 || dst_port == 443) {
        printf("HTTPS/TLS on port 443");
    } else if (src_port == 53 || dst_port == 53) {
        printf("DNS on port 53");
    } else {
        printf("Unknown");
    }
    
    printf(" - %d bytes\n", length);
    
    if (length > 0) {
        printf("Data (first %d bytes):\n", length < 64 ? length : 64);
        print_hex_dump(packet + offset, length, 64);
    }
}

void print_hex_dump(const unsigned char *data, int length, int max_bytes) {
    int bytes_to_print = length < max_bytes ? length : max_bytes;
    
    for (int i = 0; i < bytes_to_print; i += 16) {
        // Print hex
        for (int j = 0; j < 16; j++) {
            if (i + j < bytes_to_print) {
                printf("%02X ", data[i + j]);
            } else {
                printf("   ");
            }
        }
        
        // Print ASCII
        for (int j = 0; j < 16 && (i + j) < bytes_to_print; j++) {
            if (isprint(data[i + j])) {
                printf("%c", data[i + j]);
            } else {
                printf(".");
            }
        }
        
        printf("\n");
    }
}

void print_full_hex_dump(const unsigned char *data, int length) {
    printf("\n=== Full Packet Hex Dump (%d bytes) ===\n", length);
    
    for (int i = 0; i < length; i += 16) {
        printf("%04X: ", i);
        
        // Print hex
        for (int j = 0; j < 16; j++) {
            if (i + j < length) {
                printf("%02X ", data[i + j]);
            } else {
                printf("   ");
            }
        }
        
        printf(" | ");
        
        // Print ASCII
        for (int j = 0; j < 16 && (i + j) < length; j++) {
            if (isprint(data[i + j])) {
                printf("%c", data[i + j]);
            } else {
                printf(".");
            }
        }
        
        printf("\n");
    }
}

const char* get_port_name(int port) {
    switch (port) {
        case 20: return "FTP-DATA";
        case 21: return "FTP";
        case 22: return "SSH";
        case 23: return "Telnet";
        case 25: return "SMTP";
        case 53: return "DNS";
        case 80: return "HTTP";
        case 110: return "POP3";
        case 143: return "IMAP";
        case 443: return "HTTPS";
        case 3306: return "MySQL";
        case 5432: return "PostgreSQL";
        case 8080: return "HTTP-Alt";
        default: return NULL;
    }
}

void inspect_last_session() {
    if (packet_count == 0) {
        printf("\n[C-Shark] No packets in the last session. Please capture some packets first.\n");
        return;
    }
    
    printf("\n[C-Shark] Last Session Summary (%d packets captured):\n", packet_count);
    printf("================================================================\n");
    
    for (int i = 0; i < packet_count; i++) {
        struct ether_header *eth = (struct ether_header *)packets[i].data;
        int ether_type = ntohs(eth->ether_type);
        
        printf("Packet #%d | Timestamp: %ld.%06ld | Length: %d bytes",
               packets[i].id, packets[i].timestamp.tv_sec, 
               packets[i].timestamp.tv_usec, packets[i].length);
        
        if (ether_type == ETHERTYPE_IP) {
            struct ip *ip_header = (struct ip *)(packets[i].data + sizeof(struct ether_header));
            char src_ip[INET_ADDRSTRLEN];
            char dst_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(ip_header->ip_src), src_ip, INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &(ip_header->ip_dst), dst_ip, INET_ADDRSTRLEN);
            printf(" | IPv4: %s -> %s", src_ip, dst_ip);
            
            if (ip_header->ip_p == IPPROTO_TCP) {
                struct tcphdr *tcp = (struct tcphdr *)(packets[i].data + sizeof(struct ether_header) + (ip_header->ip_hl * 4));
                printf(" | TCP: %d -> %d", ntohs(tcp->th_sport), ntohs(tcp->th_dport));
            } else if (ip_header->ip_p == IPPROTO_UDP) {
                struct udphdr *udp = (struct udphdr *)(packets[i].data + sizeof(struct ether_header) + (ip_header->ip_hl * 4));
                printf(" | UDP: %d -> %d", ntohs(udp->uh_sport), ntohs(udp->uh_dport));
            }
        } else if (ether_type == ETHERTYPE_IPV6) {
            printf(" | IPv6");
        } else if (ether_type == ETHERTYPE_ARP) {
            printf(" | ARP");
        }
        
        printf("\n");
    }
    
    printf("\nEnter Packet ID to inspect in detail (or 0 to return): ");
    int packet_id;
    scanf("%d", &packet_id);
    getchar();
    
    if (packet_id == 0) {
        return;
    }
    
    // Find the packet
    int found = -1;
    for (int i = 0; i < packet_count; i++) {
        if (packets[i].id == packet_id) {
            found = i;
            break;
        }
    }
    
    if (found == -1) {
        printf("\n[C-Shark] Packet ID %d not found in the last session.\n", packet_id);
        return;
    }
    
    printf("\n");
    printf("=================================================================\n");
    printf("           DETAILED PACKET INSPECTION - Packet #%d              \n", packet_id);
    printf("=================================================================\n");
    
    // Create a fake pcap_pkthdr for reprocessing
    struct pcap_pkthdr pkthdr;
    pkthdr.ts = packets[found].timestamp;
    pkthdr.len = packets[found].length;
    pkthdr.caplen = packets[found].length;
    
    process_packet(packet_id, &pkthdr, packets[found].data, 0);
    
    print_full_hex_dump(packets[found].data, packets[found].length);
    
    printf("\n=================================================================\n");
}

void free_stored_packets() {
    for (int i = 0; i < packet_count; i++) {
        if (packets[i].data) {
            free(packets[i].data);
            packets[i].data = NULL;
        }
    }
    packet_count = 0;
}
