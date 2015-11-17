#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "networks.h"
#include "cpe464.h"

int udp_recv_setup(int port);
void initFields(Packet *pkt);
char *pktToString(Packet packet);

enum STATE {
    FILENAME, WINDOW, RECV_DATA, SENDI_ACK, SEND_EOF_ACK
};

int main (int argc, char **argv) {
    int socket;
    int port = 0;
    socket = udp_recv_setup(port);
    Packet packet;
    struct sockaddr *src_addr;
    socklen_t *addrLen;
    fd_set readFds;
    struct timeval timeout;

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    FD_ZERO(&readFds);

    while (1) {
        printf("recieving data\n");
        if (selectMod(socket + 1, &readFds, 0, 0, &timeout) < 0) {
            perror("select call");
            exit(-1);
        }
        printf("found data in select call\n");
        if (recvfrom(socket, packet.packet_data, MAX_LEN_PKT, 0, 
         src_addr, addrLen) < 0) {
            perror("recvFrom call");
            exit(-1);
        }
        initFields(&packet);
        printf("recieved packet %s\n", pktToString(packet));
    }

    return 0;
}

char *pktToString(Packet packet) {
    char *returnString;
    char *data = packet.packet_data;
    sprintf(returnString, "Packet %d, data %s", packet.seq_num, data + HDR_LEN);
    return returnString;
}

void initFields(Packet *pkt) {
    char *data = pkt->packet_data;

    memcpy(&(pkt->seq_num), data, 4);
    memcpy(&(pkt->checksum), data + 4, 2);
    memcpy(&(pkt->flag), data + 6, 1);
    memcpy(&(pkt->size), data + 7, 2);
}



int udp_recv_setup(int port)
{
    int server_socket= 0;
    struct sockaddr_in local;      /* socket address for local side  */
    socklen_t len= sizeof(local);  /* length of local address        */

    /* create the socket  */
    server_socket= socket(AF_INET, SOCK_DGRAM, 0);
    if(server_socket < 0) {
        perror("socket call");
        exit(1);
    }

    local.sin_family= AF_INET;         //internet family
    local.sin_addr.s_addr= htonl(INADDR_ANY) ; //wild card machine address
    local.sin_port= htons(port);                 //let system choose the port

    /* bind the name (address) to a port */
    if (bindMod(server_socket, (struct sockaddr *) &local, sizeof(local)) < 0) {
        perror("bind call");
        exit(-1);
    }

    //get the port name and print it out
    if (getsockname(server_socket, (struct sockaddr*)&local, &len) < 0) {
        perror("getsockname call");
        exit(-1);
    }

    printf("socket has port %d \n", ntohs(local.sin_port));
    printf("ip %s \n", inet_ntoa(local.sin_addr));

    return server_socket;
}
