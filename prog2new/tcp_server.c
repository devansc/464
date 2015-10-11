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
int main(int argc, char *argv[])
{
    int server_socket= 0;   //socket descriptor for the server socket
    int client_socket= 0;   //socket descriptor for the client socket

    printf("sockaddr: %d sockaddr_in %d\n", sizeof(struct sockaddr), sizeof(struct sockaddr_in));
    

    //create the server socket
    printf("setting up server\n");
    server_socket= tcp_recv_setup();

    //look for a client to serve
    printf("listening for client\n");
    client_socket = tcp_listen(server_socket, 5);

    while(1) {
        get_pkt(client_socket);
    }
    
    /* close the sockets */
    close(server_socket);
    close(client_socket);
    return 0;
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
        get_handle(buf, message_len);
        break;
    case 5:
        get_message(buf, message_len);
        break;
    default:
        printf("Unkown flag %hhu\n", ntohs(pkt.flag) >> 8);
        break;
    }

}

void get_handle(char *pkt, int pkt_len) {
    char *handle;
    uint8_t *handleLen;

    handleLen = (uint8_t *)pkt + NRML_HDR_LEN;
    handle = malloc(*handleLen + 1);
    memcpy(handle, handleLen + 1, *handleLen);
    handle[*handleLen] = 0;
    printf("handle recieved: %s\n", handle);
}

void get_message(char *pkt, int pkt_len) {
    printf("Message received, length: %d\n", pkt_len);
    printf("Data: %s\n", pkt + sizeof(struct nrml_hdr));
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

/* This function waits for a client to ask for services.  It returns
   the socket number to service the client on.    */

int tcp_listen(int server_socket, int back_log)
{
    int client_socket= 0;
    if (listen(server_socket, back_log) < 0) {
        perror("listen call");
        exit(-1);
    }

    if ((client_socket= accept(server_socket, (struct sockaddr*)0, (socklen_t *)0)) < 0) {
        perror("accept call");
        exit(-1);
    }
      
    return(client_socket);
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
