#include "trace.h"


void sniff_packets(pcap_t *handle) {
    const u_char *packet;
    struct pcap_pkthdr header;
    int packetNum = 0;
    struct ethr_header *pkt;
    char *typePacket;
    struct pseudo_tcp_header pth;
    uint8 len_packet;


    while (packet = pcap_next(handle, &header)) {
        packetNum++;
        printf("\nPacket number: %d  Packet Len: %d\n\n", packetNum,  header.len);

        pkt = (struct ethr_header*)(packet);
        print_ether_header(pkt);

        typePacket = get_type(pkt->ether_type);
        if (typePacket == "IP") {
            pth = print_ip_header((void*)packet);
            len_packet = get_len_ip((void*)packet + sizeof(struct ethr_header));
            //printf("len_packet = %u\n", len_packet);
            switch (pth.prtcl) {
            case 1:
                print_icmp_header((void*)packet + sizeof(struct ethr_header) + len_packet);
                break;
            case 6:
                print_tcp_header((void*)packet + sizeof(struct ethr_header)+ len_packet, &pth);
                break;
            case 17:
                print_udp_header((void*)packet + sizeof(struct ethr_header)+ len_packet);
                break;
            default:
                break;
            }
        }
        else if (typePacket == "ARP") {
            print_arp_header((void*) packet);
        } 
        else {
            printf("\t\tUnknown Type: %s\n\n", typePacket);
        }
    }
}

void print_port(uint16 port) {
    if (port == 53) {
        printf("DNS\n");
    } else if (port == 80) {
        printf("HTTP\n");
    } else {
        printf("%u\n", port);
    }
}

void print_udp_header(void * udp_hs) {
    printf("\tUDP Header\n");
    printf("\t\tSource Port: ");
    print_port(get_short(udp_hs, 0));
    printf("\t\tDest Port: ");
    print_port(get_short(udp_hs, 1));
}

void print_icmp_header(void * icmp_hs) {
    uint8 type = get_short(icmp_hs, 0) >> 8;
    printf("\tICMP Header\n");
    printf("\t\tType: ");
    if (type == 8) {
        printf("Request\n");
    } else if (type == 0) {
        printf("Reply\n");
    } else {
        printf("%u\n", type);
    }
}

char *get_flag(uint16 flag, int pos) {
    return flag & pos ? "Yes" : "No";
}

void print_tcp_header(void * tcp_header_start, struct pseudo_tcp_header *pth) {
    struct tcp_header *tcpHeader = (struct tcp_header *) tcp_header_start;
    uint16 checksum;
    uint16 *cksm_header;
    uint16 cksm_hdr_len;

    printf("\tTCP Header\n");
    //print_packet(tcp_header_start, sizeof(struct tcp_header));
    //printf("\n");

    printf("\t\tSource Port: ");
    print_port(get_short(tcp_header_start, 0));
    printf("\t\tDest Port: ");
    print_port(get_short(tcp_header_start, 1));
    printf("\t\tSequence Number:  %u\n", get_short(tcp_header_start, 2) << 16 | get_short(tcp_header_start, 3));
    printf("\t\tACK Number:  %u\n", get_short(tcp_header_start, 4) << 16 | get_short(tcp_header_start, 5));
    printf("\t\tData Offset (bytes): %u\n", (get_short(tcp_header_start, 6) >> 12) * 4);
    printf("\t\tSYN Flag: %s\n", get_flag(get_short(tcp_header_start, 6), 0x2));
    printf("\t\tRST Flag: %s\n", get_flag(get_short(tcp_header_start, 6), 0x4));
    printf("\t\tFIN Flag: %s\n", get_flag(get_short(tcp_header_start, 6), 0x1));
    printf("\t\tACK Flag: %s\n", get_flag(get_short(tcp_header_start, 6), 0x10));
    printf("\t\tWindow Size: %hu\n", get_short(tcp_header_start, 7));

    cksm_hdr_len = sizeof(struct pseudo_tcp_header) + ntohs(pth->tcp_len);
    //printf("cksm_hdr_len %u\n", cksm_hdr_len);
    //printf("pth->tcp_len %u\n", ntohs(pth->tcp_len));
    cksm_header = (uint16 *) malloc(cksm_hdr_len);
    memcpy(cksm_header, pth, sizeof(struct pseudo_tcp_header));
    memcpy(cksm_header + sizeof(struct pseudo_tcp_header) / 2, tcp_header_start, ntohs(pth->tcp_len));
    //print_packet((void*)cksm_header, cksm_hdr_len);
    //printf("\n");

    checksum = ntohs(in_cksum(cksm_header, cksm_hdr_len));
    //checksum = ntohs(in_cksum(cksm_header, sizeof(cksm_header)));
    printf("\t\tChecksum: %s (0x%04x)\n", checksum == 0 ? "Correct" : "Incorrect", get_short(tcp_header_start, 8));
    //printf("\t\tchecksum calculated: 0x%x\n", checksum);
    free(cksm_header);
}

void print_ether_header(struct ethr_header *pkt) {
    printf("\tEthernet Header\n");
    printf("\t\tDest MAC: ");
    print_mac_address(pkt->ether_dest);
    printf("\t\tSource MAC: ");
    print_mac_address(pkt->ether_source);
    printf("\t\tType: %s\n", get_type(pkt->ether_type));
    printf("\n");

}

void print_arp_header(void * eth_header_start) {
    void *arp_header_start = eth_header_start + sizeof(struct ethr_header);
    struct arp_header *arpHeader;
    uint8 opcode;

    arpHeader = (struct arp_header *) arp_header_start;

    //print_packet(arp_header_start, sizeof(struct arp_header));
    printf("\tARP header\n");
    opcode = arpHeader->opcode;
    printf("\t\tOpcode: %s\n", get_short(arp_header_start, 3) == 1 ? "Request" : "Reply");
    printf("\t\tSender MAC: ");
    print_mac_address(arpHeader->sender_mac);
    printf("\t\tSender IP: %s\n", inet_ntoa(arpHeader->s_ip));
    printf("\t\tTarget MAC: ");
    print_mac_address(arpHeader->dest_mac);
    printf("\t\tTarget IP: %s\n", inet_ntoa(arpHeader->d_ip));
    printf("\n");
}

uint16 get_short(void * hdr_start, int shortNum) {
    short hdr[shortNum + 1];
    int i;
    memcpy(hdr, hdr_start, shortNum * 2 + 2);
    return ntohs(hdr[shortNum]);
}

void print_packet(void * start, int bytes) {
    int i;
    uint8 *byt1 = (uint8*) start;
    for (i = 0; i < bytes; i += 2) {
        printf("%.2X%.2X ", *byt1, *(byt1 + 1));
        byt1 += 2;
    }
}

int get_checksum(void * start, int bytes) {
    int i, sum = 0;
    uint16 *byt2 = (uint16*) start;
    for (i = 0; i < bytes; i++) {
        sum += *byt2;
        *byt2++;
        if (sum > 0xffff) {
            sum = sum & 0xffff;
            sum += 1;
        }
    }
    return sum;
}

struct pseudo_tcp_header print_ip_header(void * eth_header_start) {
    void *ip_header_start = eth_header_start + sizeof(struct ethr_header);
    struct ip_header *ipHeader;
    uint8 vers;
    uint8 iphdr_len;
    uint8 diffserv;
    uint8 ecn;
    char *protocol;
    int checksum;

    printf("\tIP Header\n");
    //print_packet(ip_header_start, sizeof(struct ip_header));
    //printf("\n");

    ipHeader = (struct ip_header*)(ip_header_start);
    //printf("sizeof ipheader %d\n", sizeof(struct ip_header));

    vers = (ipHeader->version) >> 4; 
    iphdr_len = get_len_ip(ip_header_start); 
    printf("\t\tIP Version: %u\n", vers);
    printf("\t\tHeader Len (bytes): %u\n", iphdr_len);

    diffserv = (ipHeader->tos) >> 2;
    ecn = (ipHeader->tos) & 0x03;
    printf("\t\tTOS subfields:\n");
    printf("\t\t\tDiffserv bits: %u\n", diffserv);
    printf("\t\t\tECN bits: %u\n", ecn);

    printf("\t\tTTL: %u\n", ipHeader->ttl);
    switch (ipHeader->protocol) {
    case 1: 
        protocol = "ICMP";
        break;
    case 6: 
        protocol = "TCP";
        break;
    case 17: 
        protocol = "UDP";
        break;
    default:
        protocol = "Unknown";
        //printf("warning: unknown protocol\n");
        break;
    }
    printf("\t\tProtocol: %s\n", protocol);

    checksum = ntohs(in_cksum((uint16 *)ip_header_start, iphdr_len));
    printf("\t\tChecksum: %s (0x%04x)\n", checksum == 0 ? "Correct" : "Incorrect", ntohs(ipHeader->checksum));

    printf("\t\tSender IP: %s\n", inet_ntoa(ipHeader->s_addr));
    printf("\t\tDest IP: %s\n", inet_ntoa(ipHeader->d_addr));
    printf("\n");
    return get_pseudo_header(ipHeader);
}

uint8 get_len_ip(void * ip_hdr_start) {
    struct ip_header *ipHdr = (struct ip_header *) ip_hdr_start;
    return (ipHdr->version & 0xf) * 4;
}

struct pseudo_tcp_header get_pseudo_header(struct ip_header *ipHeader) {
    struct pseudo_tcp_header pth;
    uint16 tcp_len;
    pth.s_addr = ipHeader->s_addr;
    pth.d_addr = ipHeader->d_addr;
    pth.res = 0;
    pth.prtcl = ipHeader->protocol;
    tcp_len = ntohs(ipHeader->total_len) - (ipHeader->version & 0x0f) * 4;
    //printf("ipHeader total len %u, tcp len = %u\n", ntohs(ipHeader->total_len), tcp_len); 
    pth.tcp_len = htons(tcp_len);
    return pth;
}

char * get_type(u_short typ) {
    switch (ntohs(typ)) {
    case ETHERTYPE_ARP:
        return "ARP";
    case ETHERTYPE_IP:
        return "IP";
    default:
        return "UNKNOWN"; 
    }
}

void print_mac_address(const u_char *data) {
    int i = 0;
    for (; i < 6; i++) {
        if (i < 5)
            printf("%x:", data[i]);
        else
            printf("%x", data[i]);
    }
    printf("\n");
}

void print_address(uint32 *data) {
    int i = 0;
    for (; i < 6; i++) {
        if (i < 5)
            printf("%d:", data[i]);
        else
            printf("%d", data[i]);
    }
    printf("\n");
}



int main(int argc, char **argv) {
    pcap_t *handle;         
    char errbuf[PCAP_ERRBUF_SIZE];   


    if (argc < 2) {
        fprintf(stderr, "Need to provide input file\n");
        return 1;
    }
    handle = pcap_open_offline(argv[1], errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open pcap file %s\n", errbuf);
        return 2;
    }

    sniff_packets(handle);
    pcap_close(handle);
    return(0);
}
