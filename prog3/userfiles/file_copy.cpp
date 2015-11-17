#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "networks.h"
#include "../cpe464.h"

enum STATE {
    FILENAME, WINDOW, SEND_DATA, WAITING_ACK, SEND_EOF, EOF_ACK, DONE
};

void sendPacket(Connection server, Packet packet);
void initHeader(Packet *pkt);
Connection udp_send_setup(char *host_name, char *port);

int main(int argc, char *argv[]) {
    Connection server;   
    Packet packet;
    if (argc != 3) {
        printf("usage %s [remote-machine] [remote-port]\n", argv[0]);
        exit(-1);
    }

    packet.seq_num = 0;
    /*
    packet.checksum = 0;
    packet.flag = 0;
    packet.size = HDR_LEN + 3;
    */
    memcpy(packet.packet_data + HDR_LEN, "yo", 3);

    initHeader(&packet);

    server = udp_send_setup(argv[1], argv[2]);
    
    sendPacket(server, packet);

    return 0;
}

/*   PACKET HEADER
 *  |    4    |  2  | 1 |  2  |   DATA
 *  |  seqN   | cksm|flg| size|   DATA
 */
void initHeader(Packet *pkt) {
    char *data = pkt->packet_data;

    memcpy(data, &(pkt->seq_num), 4);
    memcpy(data + 4, &(pkt->checksum), 2);
    memcpy(data + 6, &(pkt->flag), 1);
    memcpy(data + 7, &(pkt->size), 2);
}

void sendPacket(Connection server, Packet packet) {
    printf("sending packet\n");
    if (sendto(server.socket, packet.packet_data, packet.size, 0, (struct sockaddr*) &server.address, server.addr_len) < 0) {
        perror("send call failed");
        exit(-1);
    }
    printf("sent\n");
}

Connection udp_send_setup(char *host_name, char *port) {
    Connection newConnection;
    int socket_num;
    struct sockaddr_in remote;       // socket address for remote side
    struct hostent *hp;              // address of remote host

    // create the socket
    if ((socket_num = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
	    perror("socket call");
	    exit(-1);
	}
    

    // designate the addressing family
    remote.sin_family= AF_INET;

    // get the address of the remote host and store
    if ((hp = gethostbyname(host_name)) == NULL)
	{
        printf("Error getting hostname: %s\n", host_name);
        exit(-1);
	}
    
    memcpy((char*)&remote.sin_addr, (char*)hp->h_addr, hp->h_length);

    // get the port used on the remote side and store
    remote.sin_port= htons(atoi(port));
    
    newConnection.socket = socket_num;
    newConnection.address = remote;
    newConnection.addr_len = sizeof(struct sockaddr_in);
    printf("created connection socket %d, address %s\n", socket_num, inet_ntoa(remote.sin_addr));

    return newConnection;
}
