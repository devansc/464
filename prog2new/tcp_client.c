/******************************************************************************
 * tcp_client.c
 *
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
#include "testing.h"
#include "packet.h"

#define SEQ_NUM seq_num++
#define NRML_HDR_LEN 7

int seq_num;
char *handle;

int main(int argc, char * argv[])
{
    int server_socket;         //socket descriptor
    seq_num = 0;

    /* check command line arguments  */
    if(argc!= 4)
    {
        printf("usage: %s handle host-name port-number \n", argv[0]);
        exit(1);
    }

    /* set up the socket for TCP transmission  */
    handle = strdup(argv[1]);
    if (strlen(handle) > 255) {
        printf("Handle must be less than 256 characters long\n");
    }

    server_socket = tcp_send_setup(argv[2], argv[3]);

    while (1) {
        tcp_send_recv(server_socket);
        //tcp_send_message(server_socket);
    }

        
    close(server_socket);
    return 0;
}

void create_and_send_msg(char *send_buf, int server_socket) {
    char *send_buf_dup = strdup(send_buf);
    char *token;
    char *delim = " ";
    char *toHandle;
    int myHandleLen = strlen(handle), toHandleLen, msgLen;
    char *pkt = NULL;
    uint8_t flag;
    
    msgLen = strlen(send_buf) + myHandleLen + 1; // add 1 for length bit
    token = strtok(send_buf_dup, delim);

    if (token != NULL && (strcmp(token, "%M") == 0 || strcmp(token, "%m") == 0)) { // message to a client
        toHandle = strtok(NULL, delim);
        toHandleLen = strlen(toHandle);
        msgLen += toHandleLen + 1;    
        msgLen -= 4 + toHandleLen;        // subtract the %M [handle] from totalMsgLen
        //printf("totalMsgLen %d\n", totalMsgLen);
        //printf("handle is %s\n", toHandle);
        pkt = malloc(msgLen);
        pkt[0] = (uint8_t) toHandleLen;
        memcpy(pkt + 1, toHandle, toHandleLen);
        pkt[toHandleLen + 1] = (uint8_t) myHandleLen;
        memcpy(pkt + 2 + toHandleLen, handle, myHandleLen);
        //printf("trying to put message %s\n", send_buf + 4 + toHandleLen);
        memcpy(pkt + 2 + toHandleLen + myHandleLen, send_buf + 4 + toHandleLen, strlen(send_buf) - 4 - toHandleLen);
        pkt[msgLen] = 0;
        //print_packet(pkt, totalMsgLen);
        flag = (uint8_t) 5;
    } else if (token != NULL && (strcmp(token, "%B") == 0 || strcmp(token, "%b") == 0)) { //broadcast msg
        msgLen -= 3;        // subtract the %M from totalMsgLen
        pkt = malloc(msgLen);
        pkt[0] = (uint8_t) myHandleLen;
        memcpy(pkt + 1, handle, myHandleLen);
        memcpy(pkt + 1 + myHandleLen, send_buf + 3, strlen(send_buf) - 3);
        pkt[msgLen] = 0;
        flag = (uint8_t) 4;
        printf("$: ");        // no ack for broadcast so print again
        fflush(stdout);
    } else if (token != NULL && (strcmp(token, "%L") == 0 || strcmp(token, "%l") == 0)) { // list handles
        flag = (uint8_t) 10;
        pkt = create_full_packet(flag, NULL, 0);
        tcp_send(server_socket, pkt, NRML_HDR_LEN);
        return;
    } else if (token != NULL && (strcmp(token, "%E") == 0 || strcmp(token, "%e") == 0)) { // exit
        flag = (uint8_t) 8;
        pkt = create_full_packet(flag, NULL, 0);
        tcp_send(server_socket, pkt, NRML_HDR_LEN);
    }
    else {
        printf("Invalid command\n");
    }
    if (pkt) {
        pkt = create_full_packet(flag, pkt, msgLen);
        tcp_send(server_socket, pkt, msgLen + NRML_HDR_LEN);
    }
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

void tcp_send_message(int server_socket) {
    char *send_buf;         //data buffer
    int bufsize= 0;         //data buffer size
    int send_len= 0;        //amount of data to send

    /* initialize data buffer for the packet */
    bufsize= 1024;
    send_buf = (char *) malloc(bufsize);

    send_len = 0;
    while ((send_buf[send_len] = getchar()) != '\n' && send_len < 80)
	   send_len++;

    send_buf[send_len] = '\0';

    /* initialize pkt and add it to send_buf */
    create_and_send_msg(send_buf, server_socket);
    //printf("String sent: %s \n", msgHdr);
    //printf("Amount of data sent is: %d\n", sent);
}

void tcp_send_recv(int server_socket) {
    int max_sock;
    fd_set all_sockets;
    fd_set read_sockets;


    printf("$: ");
    fflush(stdout);
    FD_ZERO(&all_sockets);
    FD_ZERO(&read_sockets);
    FD_SET(server_socket, &all_sockets);
    FD_SET(0, &all_sockets);
    max_sock = server_socket;


    while(1) {
        read_sockets = all_sockets;
        if (select(server_socket + 1, &read_sockets, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(1);
        }

        if (FD_ISSET(server_socket, &read_sockets)) {
            if (get_pkt(server_socket) != 0) {
                FD_CLR(server_socket, &all_sockets);
                exit(0);
            }
            else {
                printf("$: ");
                fflush(stdout);
            }
        }
        if (FD_ISSET(0, &read_sockets)) {
            tcp_send_message(server_socket);
        }
    }
}

char *get_handle_name(char *handleStart, uint8_t handleLen) {
    char *handle = malloc(handleLen + 1);
    memcpy(handle, handleStart, handleLen);
    handle[handleLen] = 0;
    //print_packet(handleStart, handleLen);
    //printf("handleLen %u, handle %s\n", handleLen, handle);
    return handle;
}

void get_broadcast_message(char *pktStart, int client_socket) {
    uint8_t *fromHandleLen;
    char *msg;
    char *fromHandle;
    //int handle_socket, ndxHandle;
    char *pkt = pktStart + NRML_HDR_LEN;

    fromHandleLen = (uint8_t *)pkt;
    msg = strdup(pkt + *fromHandleLen + 1);
    //printf("toHandleLen %u fromHandleLen %u\n", *toHandleLen, *fromHandleLen);
    fromHandle = get_handle_name(pkt +1, *fromHandleLen);
    printf("%s [broadcast]: %s\n", fromHandle, msg);
}

void get_message(char *pkt, int client_socket) {
    uint8_t *toHandleLen, *fromHandleLen;
    char *msg;
    //char *toHandle, *fromHandle;

    toHandleLen = (uint8_t *)pkt;
    fromHandleLen = (uint8_t *)(pkt + 1 + *toHandleLen);
    msg = strdup(pkt + *toHandleLen + *fromHandleLen + 2);
    //printf("toHandleLen %u fromHandleLen %u\n", *toHandleLen, *fromHandleLen);
    printf("%s: %s\n", get_handle_name(pkt + *toHandleLen + 2, *fromHandleLen), msg);
}

void recieve_handle_packet(uint8_t *handles, int totalLen) {
    int curPos = 0;
    uint8_t len_handle;
    char *curHandle;

    printf("Handles: ");
    while (curPos < totalLen) {
        len_handle = handles[curPos++];
        curHandle = malloc(len_handle + 1);
        memcpy(curHandle, handles+curPos, len_handle);
        curHandle[len_handle] = 0;
        printf("%s, ", curHandle);
        curPos += len_handle;
    }
    printf("\n");
}

int get_pkt(int socket) {
    int message_len = 0;     //length of the received message
    char *buf;              //buffer for receiving from client
    int buffer_size= 1024;  //packet size variable
    struct nrml_hdr pkt;
    int knownHandles;

    //create packet buffer
    buf = (char *) malloc(buffer_size);
    //now get the data on the socket
    if ((message_len = recv(socket, buf, buffer_size, 0)) < 0)
    {
        perror("recv call");
        exit(-1);
    } else if (message_len == 0) {   // server exitted
        close(socket);
        printf("Server Terminated\n");
        return socket;     
    }

    
    memcpy(&pkt, buf, 7);
    //printf("Recieved packet: seqNum = %d, len = %hu, flag = %hhu\n", ntohs(pkt.seqNum), ntohs(pkt.len), ntohs(pkt.flag) >> 8);
    //print_packet(buf, message_len);
    switch (pkt.flag) {
    case 2:
        printf("Welcome %s\n", handle);
        break;
    case 3: 
        printf("Handle already in use: %s\n", handle);
        exit(1);
    case 4: 
        get_broadcast_message(buf, socket);
        break;
    case 5:
        get_message(buf + NRML_HDR_LEN, socket);
        break;
    case 6:
        //printf("recieved ack: %s\n", get_handle_name(buf+NRML_HDR_LEN + 1, (uint8_t)*(buf +NRML_HDR_LEN)));
        break;
    case 7:
        printf("Client with handle %s does not exist.\n", get_handle_name(buf + NRML_HDR_LEN + 1, (uint8_t)*(buf + NRML_HDR_LEN)));
        break;
    case 9:
        close(socket);
        return socket;;
    case 11:
        memcpy(&knownHandles, buf + NRML_HDR_LEN, 4);
        //printf("There are %d handles known by server\n", ntohl(knownHandles));
        break;
    case 12:
        //print_packet(buf+NRML_HDR_LEN, message_len - NRML_HDR_LEN);
        recieve_handle_packet((uint8_t *) (buf + NRML_HDR_LEN), message_len - NRML_HDR_LEN);
        break;
    default:
        printf("Unkown flag %hhu\n", (uint8_t)(ntohs(pkt.flag) >> 8));
        break;
    }
    return 0;
}


int tcp_send_setup(char *host_name, char *port)
{
    int socket_num;
    struct sockaddr_in remote;       // socket address for remote side
    struct hostent *hp;              // address of remote host
    char *pktTail;
    char *fullPkt;
    int fullPktLen;
    int sent = 0;

    // create the socket
    if ((socket_num = socket(AF_INET, SOCK_STREAM, 0)) < 0)
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

    if(connect(socket_num, (struct sockaddr*)&remote, sizeof(struct sockaddr_in)) < 0)
    {
        perror("connect call");
        exit(-1);
    }

    //printf("len of handle %d\n", (int)strlen(handle));
    fullPktLen = strlen(handle) + 1;
    pktTail = malloc(fullPktLen);
    pktTail[0] = (uint8_t)(fullPktLen - 1);
    memcpy(pktTail + 1, handle, strlen(handle));   
    fullPkt = create_full_packet((int8_t)1, pktTail, fullPktLen);
    fullPktLen = fullPktLen + NRML_HDR_LEN;

    sent =  send(socket_num, fullPkt, fullPktLen, 0);
    if(sent < 0)
    {
        perror("send call");
        exit(-1);
    }

    get_pkt(socket_num);

    return socket_num;
}

void send_exit() {
        
}

char *create_full_packet(uint8_t flag, char *pktTail, int pktTailLen) {
    int16_t len = NRML_HDR_LEN + pktTailLen;
    struct nrml_hdr pkt;
    char *fullPkt;

    pkt.seqNum = htons(seq_num);
    pkt.len = htons(len);
    pkt.flag = flag;
    fullPkt = malloc(len);
    memcpy(fullPkt, &pkt, NRML_HDR_LEN);
    memcpy(fullPkt + NRML_HDR_LEN, pktTail, pktTailLen);
    //printf("created packet ");
    //print_packet(fullPkt, len);
    seq_num++;
    return fullPkt;
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
