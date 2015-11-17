#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "networks.h"

/*   PACKET HEADER
 *  |    4    |  2  | 1 |  2  |   DATA
 *  |  seqN   | cksm|flg| size|   DATA
 */
void initHeader(Packet *pkt) {
    char *data = pkt->payload;
    uint32_t seq_num = htonl(pkt->seq_num);
    uint16_t size = htons(pkt->size);

    memcpy(data, &seq_num, 4);
    memcpy(data + 4, &(pkt->checksum), 2);
    memcpy(data + 6, &(pkt->flag), 1);
    memcpy(data + 7, &size, 2);
}

Packet createPacket(uint32_t seq_num, int flag, char *payload, int size_payload) {
    Packet packet;

    packet.seq_num = seq_num;
    packet.flag = (int8_t) flag;
    packet.size = size_payload + HDR_LEN;
    memset(packet.payload, 0, MAX_LEN_PKT);
    memcpy(packet.payload + HDR_LEN, payload, size_payload);
    
    printf("created packet %d, size %d, data %s\n", seq_num, packet.size, packet.payload + HDR_LEN);
    initHeader(&packet);
    return packet;
}

Packet fromPayload(char *payload) {
    Packet pkt;

    memcpy(&(pkt.seq_num), payload, 4);
    memcpy(&(pkt.checksum), payload + 4, 2);
    memcpy(&(pkt.flag), payload + 6, 1);
    memcpy(&(pkt.size), payload + 7, 2);
    pkt.size = ntohs(pkt.size);
    pkt.seq_num = ntohl(pkt.seq_num);
    memcpy(pkt.payload, payload, pkt.size);
    return pkt;
}
