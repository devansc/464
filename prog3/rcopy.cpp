#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "networks.h"
#include "rcopy.h"
#include "cpe464.h"


int main(int argc, char *argv[]) {
    Connection server;   
    state curState;

    if (argc != 5) {
        printf("usage %s [local-file] [remote-file] [remote-machine] [remote-port]\n", argv[0]);
        exit(-1);
    }

    sendErr_init(0, DROP_OFF, FLIP_OFF, DEBUG_ON, RSEED_OFF);

    server = udp_send_setup(argv[3], argv[4]);
    
    curState = FILENAME;
    
    while (curState != DONE) {
        switch (curState) {

        case FILENAME:
            curState = sendFilename(server, argv[1], argv[2]);
            break;

        case WINDOW:
            break;

        case DATA:
            break;

        case ACK:
            break;

        case EOFCONFIRM:
            break;

        case DONE:
            break;
        }
    }

    printf("done\n");
    return 0;
}

state sendFilename(Connection server, char *localFile, char *remoteFile) {
    Packet packet;
    packet = createPacket(0, 1, "yo", 3);
    sendPacket(server, packet);
    return DONE;
}

void sendPacket(Connection server, Packet packet) {
    //printf("sending packet %s\n", packet.payload + HDR_LEN);
    printf("sending packet");
    print_packet(packet.payload, packet.size);
    if (sendtoErr(server.socket, packet.payload, packet.size, 0, (struct sockaddr*) &server.address, server.addr_len) < 0) {
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

void print_packet(void * start, int bytes) {
    int i;
    uint8_t *byt1 = (uint8_t*) start;
    for (i = 0; i < bytes; i += 2) {
        printf("%.2X%.2X ", *byt1, *(byt1 + 1));
        byt1 += 2;
    }
    printf("\n");
}
