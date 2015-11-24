#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>

#include "networks.h"
#include "cpe464.h"

/*   PACKET HEADER
 *  |    4    |  2  | 1 |  2  |   DATA
 *  |  seqN   | cksm|flg| size|   DATA
 */
void initHeader(Packet *pkt) {
    char *pload = pkt->payload;
    uint32_t seq_num = htonl(pkt->seq_num);

    memcpy(pload, &seq_num, 4);
    memcpy(pload + 4, &(pkt->checksum), 2);
    memcpy(pload + 6, &(pkt->flag), 1);
}

Packet createPacket(uint32_t seq_num, int flag, unsigned char *payload, int size_payload) {
    Packet packet;
    int sizePacket;

    sizePacket = size_payload + HDR_LEN;

    packet.seq_num = seq_num;
    packet.flag = (int8_t) flag;
    packet.payload = (char *) (malloc(sizePacket));
    packet.size = sizePacket;
    memset(packet.payload, 0, sizePacket);
    if (size_payload > 0){
        memcpy(packet.payload + HDR_LEN, payload, size_payload);
        packet.data = packet.payload + HDR_LEN;
    } else {
        packet.data = NULL;
    }
    
    initHeader(&packet);
    //printf("created packet %d, size %d, data %s\n", seq_num, packet.size, packet.data);
    return packet;
}

void print_packet(void * start, int bytes) {
    int i;
    uint8_t *byt1 = (uint8_t*) start;
    for (i = 0; i < bytes; i += 2) {
        printf("%.2X%.2X ", *byt1, *(byt1 + 1));
        byt1 += 2;
    }
    printf("\n");
}

void sendPacket(Connection connection, Packet packet) {
    //printf("sending packet to %s\n", inet_ntoa(connection.address.sin_addr));
    //print_packet(packet.payload, packet.size);
    if (sendtoErr(connection.socket, packet.payload, packet.size, 0, (struct sockaddr*) &connection.address, connection.addr_len) < 0) {
        perror("send call failed");
        exit(-1);
    }
    //printf("sent\n");
}


SELECTVAL selectCall(int socket, int timeoutSec) {
    fd_set readFds;
    struct timeval timeout;
    int selectReturn;

    timeout.tv_sec = timeoutSec;
    timeout.tv_usec = 0;
    FD_ZERO(&readFds);
    FD_SET(socket, &readFds);

    //printf("recieving data\n");
    if ((selectReturn = selectMod(socket + 1, &readFds, 0, 0, &timeout)) < 0) {
        perror("select call");
        exit(-1);
    }
    if (selectReturn == 0) {
        //printf("timed out\n");
        return SELECT_TIMEOUT;
    }
    return SELECT_HAS_DATA;

}

Packet fromPayload(char *payload, int size) {
    Packet pkt;

    memcpy(&(pkt.seq_num), payload, 4);
    memcpy(&(pkt.checksum), payload + 4, 2);
    memcpy(&(pkt.flag), payload + 6, 1);
    //memcpy(&(pkt.size), payload + 7, 2);

    pkt.payload = (char *)malloc(size);
    memcpy(pkt.payload, payload, size);
    if (size > HDR_LEN)
        pkt.data = pkt.payload + HDR_LEN;
    else
        pkt.data = NULL;
    pkt.size = size;
    //pkt.size = ntohs(pkt.size);
    pkt.seq_num = ntohl(pkt.seq_num);
    return pkt;
}

Packet recievePacket(Connection *connection) {
    char payload[MAX_LEN_PKT];
    int message_len;


    //printf("connection before recieve %s\n", inet_ntoa(connection->address.sin_addr));
    if ((message_len = recvfromErr(connection->socket, payload, MAX_LEN_PKT, 0, (struct sockaddr *) &(connection->address), &(connection->addr_len))) < 0) {
        perror("recvFrom call");
        exit(-1);
    } else if (message_len == 0) {
        exit(0);   // client exitted
    }
    //printf("port now %d\n", connection->address.sin_port);
    //printf("connection after recieve %s\n", inet_ntoa(connection->address.sin_addr));
    printf("recieved message len %d\n", message_len);
    return fromPayload(payload, message_len);
}
