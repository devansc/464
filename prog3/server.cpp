#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "networks.h"
#include "server.h"
#include "cpe464.h"

int main (int argc, char **argv) {
    int socket;
    int port = 0;
    socket = udp_recv_setup(port);
    fd_set readFds;
    struct timeval timeout;
    int selectReturn;

    while (1) {
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        FD_ZERO(&readFds);
        FD_SET(socket, &readFds);

        printf("recieving data\n");
        if ((selectReturn = selectMod(socket + 1, &readFds, 0, 0, &timeout)) < 0) {
            perror("select call");
            exit(-1);
        }
        if (selectReturn == 0) {
            printf("timed out\n");
            continue;   // timed out
        }

        // found data to be recieved
        printf("found data in select call\n");
        if (fork() == 0) {
            startClient(socket);
            break;
        }
    }

    printf("child exitting\n");
    return 0;
}

void startClient(int socket) {
    STATE curState;

    curState = FILENAME;

    while (curState != DONE) {
        switch (curState) {

        case FILENAME:
            curState = recieveFilename(socket);
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

}

STATE recieveFilename(int socket) {
    Packet packet;
    char payload[MAX_LEN_PKT];
    struct sockaddr src_addr;
    socklen_t addrLen;
    int message_len;

    if ((message_len = recvfrom(socket, payload, MAX_LEN_PKT, 0, 
     &src_addr, &addrLen)) < 0) {
        perror("recvFrom call");
        exit(-1);
    } else if (message_len == 0) {
        exit(0);   // client exitted
    }
    packet = fromPayload(payload);
    printf("recieved filename %s\n", pktToString(packet));

    return DONE;
}



char *pktToString(Packet packet) {
    char *returnString;
    char *data = packet.payload;
    sprintf(returnString, "Packet %d len %d, data %s", packet.seq_num, packet.size, data + HDR_LEN);
    return returnString;
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
