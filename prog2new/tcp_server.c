/******************************************************************************
 * tcp_server.c
 *
 * CPE 464 - Program 1
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "networks.h"
#include "packet.h"

#define NRML_HDR_LEN 7

char *handles[5];
int handleSockets[5];
int curHandle = 0;

int main(int argc, char *argv[])
{
    int server_socket= 0;   //socket descriptor for the server socket
    //int listener_socket = 0;   //socket descriptor for the listener socket

    //printf("sockaddr: %d sockaddr_in %d\n", sizeof(struct sockaddr), sizeof(struct sockaddr_in));

    //create the server socket
    server_socket= tcp_recv_setup();

    //look for a client to serve
    tcp_listen(server_socket, 5);

    send_recieve_packets(server_socket);
    
    /* close the sockets */
    close(server_socket);
    //close(client_socket);
    return 0;
}

void send_recieve_packets(int server_socket) {
    int max_sock, min_sock, sockNdx, client_socket;
    fd_set all_sockets;
    fd_set read_sockets;

    FD_ZERO(&all_sockets);
    FD_ZERO(&read_sockets);


    FD_SET(server_socket, &all_sockets);
    max_sock = server_socket;
    min_sock = server_socket;


    while(1) {
        read_sockets = all_sockets;
        if (select(max_sock + 1, &read_sockets, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(1);
        }

        for (sockNdx = min_sock; sockNdx <= max_sock; sockNdx++) {
            if (FD_ISSET(sockNdx, &read_sockets)) {
                if (sockNdx == server_socket) {
                    if ((client_socket = accept(server_socket, (struct sockaddr*)0, (socklen_t *)0)) < 0) {
                        perror("accept call");
                        exit(-1);
                    }
                    FD_SET(client_socket, &all_sockets);
                    max_sock = client_socket;
                } else {
                    get_pkt(sockNdx);
                }
            }
        }

    }
}

void get_pkt(int client_socket) {
    int message_len = 0;     //length of the received message
    char *buf;              //buffer for receiving from client
    int buffer_size= 1024;  //packet size variable
    struct nrml_hdr pkt;

    //create packet buffer
    buf = (char *) malloc(buffer_size);
    //now get the data on the client_socket
    if ((message_len = recv(client_socket, buf, buffer_size, 0)) < 0)
    {
        perror("recv call");
        exit(-1);
    }
    
    memcpy(&pkt, buf, 7);
    //printf("Recieved packet: seqNum = %d, len = %hu, flag = %hhu\n", ntohs(pkt.seqNum), ntohs(pkt.len), ntohs(pkt.flag) >> 8);
    //print_packet(buf, message_len);
    switch (ntohs(pkt.flag) >> 8) {
    case 1:
        get_handle(buf + NRML_HDR_LEN, message_len, client_socket);
        break;
    case 5:
        get_message(buf + NRML_HDR_LEN, client_socket);
        break;
    default:
        printf("Unkown flag %hhu\n", ntohs(pkt.flag) >> 8);
        break;
    }

}

char *get_handle_name(char *handleStart, uint8_t handleLen) {
    char *handle = malloc(handleLen + 1);
    memcpy(handle, handleStart, handleLen);
    handle[handleLen] = 0;
    return handle;
}

void get_handle(char *pkt, int pkt_len, int client_socket) {
    char *handle;
    uint8_t *handleLen;

    handleLen = (uint8_t *)pkt;
    handle = malloc(*handleLen + 1);
    memcpy(handle, handleLen + 1, *handleLen);
    handle[*handleLen] = 0;
    //printf("%s entered the chat room\n", handle);

    handles[curHandle] = handle;
    handleSockets[curHandle] = client_socket;
    curHandle++;
}

void get_message(char *pkt, int client_socket) {
    uint8_t *toHandleLen, *fromHandleLen;
    char *msg;
    //char *toHandle, *fromHandle;

    toHandleLen = (uint8_t *)pkt;
    fromHandleLen = (uint8_t *)(pkt + 1 + *toHandleLen);
    msg = strdup(pkt + *toHandleLen + *fromHandleLen + 2);
    //printf("toHandleLen %u fromHandleLen %u\n", *toHandleLen, *fromHandleLen);
    printf("%s->%s: %s\n", get_handle_name(pkt + *toHandleLen + 2, *fromHandleLen), get_handle_name(pkt + 1, *toHandleLen), msg);
}

/* This function sets the server socket.  It lets the system
   determine the port number.  The function returns the server
   socket number and prints the port number to the screen.  */

int tcp_recv_setup()
{
    int server_socket= 0;
    struct sockaddr_in local;      /* socket address for local side  */
    socklen_t len= sizeof(local);  /* length of local address        */

    /* create the socket  */
    server_socket= socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket < 0) {
        perror("socket call");
        exit(1);
    }

    local.sin_family= AF_INET;         //internet family
    local.sin_addr.s_addr= INADDR_ANY; //wild card machine address
    local.sin_port= htons(0);                 //let system choose the port

    /* bind the name (address) to a port */
    if (bind(server_socket, (struct sockaddr *) &local, sizeof(local)) < 0) {
        perror("bind call");
        exit(-1);
    }

    //get the port name and print it out
    if (getsockname(server_socket, (struct sockaddr*)&local, &len) < 0) {
        perror("getsockname call");
        exit(-1);
    }

    printf("socket has port %d \n", ntohs(local.sin_port));

    return server_socket;
}

int tcp_listen(int server_socket, int back_log)
{
    int listener;
    if ((listener = listen(server_socket, back_log)) < 0) {
        perror("listen call");
        exit(-1);
    }

    /*
    if ((client_socket= accept(server_socket, (struct sockaddr*)0, (socklen_t *)0)) < 0) {
        perror("accept call");
        exit(-1);
    }
    */
      
    return listener;
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
