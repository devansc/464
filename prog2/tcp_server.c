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
#define MAX_HANDLES 100

char *handles[MAX_HANDLES];
int handleSockets[MAX_HANDLES];
int curHandle = 0;
int handlesInUse[MAX_HANDLES];
int seq_num;

int main(int argc, char *argv[])
{
    int i = 0;
    int server_socket= 0;   //socket descriptor for the server socket
    int port = 0;
    seq_num = 0;

    for ( ; i < MAX_HANDLES; i++) 
        handlesInUse[i] = 0;

    if (argc > 1) {
        port = atoi(argv[1]);
    }

    server_socket= tcp_recv_setup(port);

    //look for a client to serve
    tcp_listen(server_socket, 5);

    send_recieve_packets(server_socket);
    
    // never get here
    close(server_socket);
    return 0;
}

void tcp_send(int socket, char *data, int len_data) {
    int sent= 0;            //actual amount of data sent
    sent =  send(socket, data, len_data, 0);
    if(sent < 0)
    {
        perror("send call");
        exit(-1);
    }
    //printf("Amount of data sent is: %d\n", sent);
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
                    if (client_socket > max_sock)
                        max_sock = client_socket;
                } else {
                    if (get_pkt(sockNdx) != 0) {
                        FD_CLR(sockNdx, &all_sockets);
                        //printf("removed socket %d from all_sockets\n", sockNdx);
                    }
                }
            }
        }

    }
}

// have to check for -1 return value
void close_socket(int socket) {
    int i;
    for (i = 0; i < curHandle; i++) {
        if (handleSockets[i] == socket) {
            handlesInUse[i] = 0;
            close(socket);
        }
    }
}


void send_handles(int client_socket) {
    int numHandles = 0, i, networkNumHandles, lenHandlePkt = 0, curPos = 0;
    uint8_t handleLen;
    char *numHandlePkt, *handlePkt, *fullHandlePkt;
    for (i = 0; i < curHandle; i++) {
        if (handlesInUse[i]) {
            numHandles++;
            lenHandlePkt += strlen(handles[i]);
        }
    }
    //printf("server knows %d handles\n", numHandles);
    networkNumHandles = htonl(numHandles);
    numHandlePkt = create_full_packet((uint8_t)11, (void *)&networkNumHandles, 4);
    tcp_send(client_socket, numHandlePkt, NRML_HDR_LEN + 4);
    
    lenHandlePkt += numHandles;      // add the 1 bit for length of each handle
    handlePkt = malloc(lenHandlePkt);
    for (i = 0; i < curHandle; i++) {
        if (handlesInUse[i]) {
            handleLen = (uint8_t) strlen(handles[i]);
            handlePkt[curPos++] = handleLen;
            memcpy(handlePkt + curPos, handles[i], handleLen);
            curPos += handleLen;
        }
    }

    fullHandlePkt = create_full_packet((uint8_t)12, handlePkt, lenHandlePkt);
    fullHandlePkt[4] = 0;  // set the length field of 
    fullHandlePkt[5] = 0;  // the packet to 0
    usleep(100);
    tcp_send(client_socket, fullHandlePkt, NRML_HDR_LEN + lenHandlePkt);
    free(fullHandlePkt);
}

int get_pkt(int client_socket) {
    int message_len = 0;     //length of the received message
    char *buf;              //buffer for receiving from client
    int buffer_size= MAX_PKT_LEN;  //packet size variable
    struct nrml_hdr pkt;

    //create packet buffer
    buf = (char *) malloc(buffer_size);
    memset(buf, 0, buffer_size);
    //now get the data on the client_socket
    if ((message_len = recv(client_socket, buf, buffer_size, 0)) < 0)
    {
        perror("recv call");
        exit(-1);
    } else if (message_len == 0) {   //client exitted
        close_socket(client_socket);
        //printf("client pressed CTRL-C\n");
        return client_socket;
    }
    
    memcpy(&pkt, buf, 7);
    //printf("Recieved packet: seqNum = %d, len = %hu, flag = %hhu\n", ntohs(pkt.seqNum), ntohs(pkt.len), ntohs(pkt.flag) >> 8);
    //print_packet(buf, message_len);
    switch (ntohs(pkt.flag) >> 8) {
    case INIT_MSG:
        get_handle(buf + NRML_HDR_LEN, message_len, client_socket);
        break;
    case BRDCST_MSG:
        forward_broadcast_message(buf, client_socket);
        break;
    case REG_MSG:
        get_message(buf, client_socket);
        break;
    case EXIT_MSG:
        //printf("client exitting %d\n", client_socket);
        tcp_send(client_socket, create_full_packet((uint8_t)9, NULL, 0), NRML_HDR_LEN);
        close_socket(client_socket);
        return client_socket;
    case HNDL_REQUEST:
        send_handles(client_socket);
        break;
    default:
        printf("Unkown flag %hhu\n", (uint8_t)(ntohs(pkt.flag) >> 8));
        break;
    }
    free(buf);
    return 0;
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
    int i, ndxHandle;

    handleLen = (uint8_t *)pkt;
    handle = malloc(*handleLen + 1);
    memcpy(handle, handleLen + 1, *handleLen);
    handle[*handleLen] = 0;
    //printf("%s entered the chat room\n", handle);
    
    for (i = 0; i < curHandle; i++) {
        if (strcmp(handles[i], handle) == 0 && handlesInUse[i]) {
            //printf("handle %s already exists\n", handle);
            pkt = create_full_packet(3, NULL, 0);            
            tcp_send(client_socket, pkt, NRML_HDR_LEN);
            free(pkt);
            return;
        }
    }

    pkt = create_full_packet(2, NULL, 0);            
    tcp_send(client_socket, pkt, NRML_HDR_LEN);
    free(pkt);

    ndxHandle = curHandle;
    for (i = 0; i < curHandle; i++) {
        if (!handlesInUse[i]) {
            ndxHandle = i;
            break;
        }
    }

    handles[ndxHandle] = handle;
    handleSockets[ndxHandle] = client_socket;
    handlesInUse[ndxHandle] = 1;            // need to set to 0 when they exit / close
    //printf("added handle at ndx %d %s\n", ndxHandle, handle);
    curHandle = curHandle == ndxHandle ? curHandle + 1 : curHandle;
}

char *create_full_packet(uint8_t flag, char *pktTail, int pktTailLen) {
    int16_t len = NRML_HDR_LEN + pktTailLen;
    struct nrml_hdr pkt;
    char *fullPkt;
    pkt.seqNum = htons(seq_num);
    pkt.len = htons(len);
    pkt.flag = flag;
    fullPkt = malloc(len);
    memset(fullPkt, 0, len);
    memcpy(fullPkt, &pkt, NRML_HDR_LEN);
    memcpy(fullPkt + NRML_HDR_LEN, pktTail, pktTailLen);
    //printf("created packet ");
    //print_packet(fullPkt, len);
    seq_num++;
    return fullPkt;
}

int get_handle_socket(char *handle) {
    int i;
    for (i = 0; i < curHandle; i++) {
        if (strcmp(handles[i], handle) == 0 && handlesInUse[i]) {
            //printf("found handle socket %s %d", handle, handleSockets[i]);
            return handleSockets[i];
        }
    }
    return -1;
}

void forward_broadcast_message(char *pktStart, int client_socket) {
    uint8_t *fromHandleLen;
    char *msg;
    char *fromHandle;
    int handle_socket, ndxHandle;
    char *pkt = pktStart + NRML_HDR_LEN;

    fromHandleLen = (uint8_t *)pkt;
    msg = strdup(pkt + *fromHandleLen + 1);
    //printf("toHandleLen %u fromHandleLen %u\n", *toHandleLen, *fromHandleLen);
    fromHandle = get_handle_name(pkt +1, *fromHandleLen);
    //printf("%s [broadcast]: %s\n", fromHandle, msg);

    for (ndxHandle = 0; ndxHandle < curHandle; ndxHandle++ ) {
        handle_socket = handleSockets[ndxHandle];
        if (handle_socket != client_socket && handlesInUse[ndxHandle])
            tcp_send(handle_socket, pktStart, strlen(pkt) + NRML_HDR_LEN);
    }
    if (ndxHandle == 0) {
        //printf("No handles found, not forwarding packet\n");
    }
}

void get_message(char *pktStart, int client_socket) {
    uint8_t *toHandleLen, *fromHandleLen;
    char *msg;
    char *toHandle, *fromHandle;
    int handle_socket;
    char *pkt = pktStart + NRML_HDR_LEN;
    char *responsePacket, *responsePacketTail;

    toHandleLen = (uint8_t *)pkt;
    fromHandleLen = (uint8_t *)(pkt + 1 + *toHandleLen);
    msg = strdup(pkt + *toHandleLen + *fromHandleLen + 2);
    //printf("toHandleLen %u fromHandleLen %u\n", *toHandleLen, *fromHandleLen);
    fromHandle = get_handle_name(pkt + *toHandleLen + 2, *fromHandleLen);
    toHandle = get_handle_name(pkt + 1, *toHandleLen);
    //printf("%s: %s\n", fromHandle, msg);

    handle_socket = get_handle_socket(toHandle);
    if (handle_socket != -1) {
        tcp_send(handle_socket, pktStart, strlen(pkt) + NRML_HDR_LEN);

        responsePacketTail = malloc(*toHandleLen + 1);
        responsePacketTail[0] = *toHandleLen;
        memcpy(responsePacketTail + 1, toHandle, *toHandleLen);

        responsePacket = create_full_packet(6, responsePacketTail, 1 + *toHandleLen);
        tcp_send(client_socket, responsePacket, NRML_HDR_LEN + *toHandleLen + 1);
        //printf("recieved pkt len %lu\n", strlen(pkt) + NRML_HDR_LEN);
        fflush(stdout);
    }
    else {
        //printf("Handle %s (len %u) not found, not forwarding packet\n", toHandle, *toHandleLen);

        responsePacketTail = malloc(*toHandleLen + 1);
        responsePacketTail[0] = *toHandleLen;
        memcpy(responsePacketTail + 1, toHandle, *toHandleLen);

        responsePacket = create_full_packet(7, responsePacketTail, *toHandleLen + 1);
        tcp_send(client_socket, responsePacket, NRML_HDR_LEN + *toHandleLen + 1);
    }
    free(responsePacket);
}

/* This function sets the server socket.  It lets the system
   determine the port number.  The function returns the server
   socket number and prints the port number to the screen.  */

int tcp_recv_setup(int port)
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
    local.sin_port= htons(port);                 //let system choose the port

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
