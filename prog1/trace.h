#ifndef TRACE_H
#define TRACE_H

#include <pcap/pcap.h>
#include <stdio.h>
#include <netinet/if_ether.h>
#include <net/ethernet.h>
#include <netinet/ether.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "checksum.c"

#define ether_addr_len 6
#define uint8 unsigned char
#define uint16 unsigned short
#define uint32 unsigned int

#pragma pack(1)


struct ethr_header {
	u_char ether_dest[ether_addr_len];
	u_char ether_source[ether_addr_len];
	u_short ether_type;
}__attribute__((packed));

struct arp_header {
	uint16 hdwr_type;
	uint16 prtcl_type;
    uint8 h_size;
    uint8 p_size;
    uint16 opcode;
	uint8 sender_mac[ether_addr_len];
	struct in_addr s_ip;
	uint8 dest_mac[ether_addr_len];
	struct in_addr d_ip;
}__attribute__((packed));

struct tcp_header {
    uint16 src_prt;
    uint16 dst_prt;
    uint32 seq_num;
    uint32 ack_num;
    uint16 hdr_len_flags;
    uint16 wndw_size;
    uint16 checksum;
}__attribute__((packed));

struct ip_header {
	uint8 version;	
	uint8 tos;
	uint16 total_len;
	uint16 id;
	uint16 flags;
	uint8 ttl;
	uint8 protocol;
	uint16 checksum;
	struct in_addr s_addr;
	struct in_addr d_addr;
}__attribute__((packed));

struct pseudo_tcp_header {
    struct in_addr s_addr;
    struct in_addr d_addr;
    uint8 res;
    uint8 prtcl;
    uint16 tcp_len;
}__attribute__((packed));


void print_mac_address(const u_char *data);
void print_address(uint32 *data);
char * get_type(u_short typ);
void print_ether_header(struct ethr_header *pkt);
struct pseudo_tcp_header print_ip_header(void *pkt);
void print_arp_header(void * eth_header_start);
void print_tcp_header(void * tcp_header_start, struct pseudo_tcp_header *pth);
void print_icmp_header(void * icmp_hs);
void print_udp_header(void * udp_hs);

void print_port(uint16 port);
uint16 get_short(void * hdr_start, int shortNum);
void print_packet(void * start, int bytes);
char *get_flag(uint16 flag, int pos);
uint8 get_len_ip(void * ip_hdr_start);
struct pseudo_tcp_header get_pseudo_header(struct ip_header *ipHeader);

#endif
