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


int seq_num;
char *handle;

int main(int argc, char * argv[])
{
    int server_socket;         //socket descriptor
    seq_num = 0;
    //setvbuf(stdin, (char *)NULL, _IOFBF, 0);

    /* check command line arguments  */
    if(argc!= 4)
    {
        printf("usage: %s handle host-name port-number \n", argv[0]);
        exit(1);
    }

    /* set up the socket for TCP transmission  */
    handle = strdup(argv[1]);
    if (strlen(handle) > 256) {
        printf("Handle must be less than 256 characters long\n");
    }

    server_socket = tcp_send_setup(argv[2], argv[3]);

    while (1) {
        tcp_send_recv(server_socket);
    }
        
    close(server_socket);
    return 0;
}

void send_msg_pkts(int server_socket, int flag, char *pktHandleHeader, int pktHandleHeaderLen, char *message, int messageLen) {
    
    int totalMsgLen = NRML_HDR_LEN + pktHandleHeaderLen + messageLen;
    char *totalPkt, *endPacket;
    int curSendMsgLen;

    if (totalMsgLen <= MAX_PKT_LEN) {
        totalPkt = malloc(pktHandleHeaderLen + messageLen + 1);
        memset(totalPkt, 0, pktHandleHeaderLen + messageLen + 1);
        memcpy(totalPkt, pktHandleHeader, pktHandleHeaderLen);
        memcpy(totalPkt + pktHandleHeaderLen, message, messageLen);
        totalPkt[pktHandleHeaderLen + messageLen] = 0;

        totalPkt = create_full_packet((uint8_t)flag, totalPkt, pktHandleHeaderLen + messageLen);
        tcp_send(server_socket, totalPkt, totalMsgLen);
        free(totalPkt);
        return;
    }

    while (messageLen > 0) {
        curSendMsgLen = MAX_PKT_LEN -  NRML_HDR_LEN - pktHandleHeaderLen ;
        curSendMsgLen = messageLen < curSendMsgLen ? messageLen : curSendMsgLen - 1;
        endPacket = malloc(curSendMsgLen + pktHandleHeaderLen + 1);  // 1 is for null
        memset(endPacket, 0, curSendMsgLen + pktHandleHeaderLen + 1);
        memcpy(endPacket, pktHandleHeader, pktHandleHeaderLen);
        memcpy(endPacket + pktHandleHeaderLen, message, curSendMsgLen);
        endPacket[curSendMsgLen + pktHandleHeaderLen] = 0;

        totalPkt = create_full_packet((uint8_t)flag, endPacket, curSendMsgLen + pktHandleHeaderLen + 1);
        //printf("packet is len %d\n", curSendMsgLen + pktHandleHeaderLen + 1);
        //print_packet(totalPkt, curSendMsgLen + pktHandleHeaderLen + NRML_HDR_LEN + 1);
        tcp_send(server_socket, totalPkt, curSendMsgLen + pktHandleHeaderLen + NRML_HDR_LEN + 1);
        message += curSendMsgLen;
        messageLen -= curSendMsgLen;
    }
}

void send_msg_pkt(int server_socket, char *send_buf, char *toHandle) {
    int toHandleLen = strlen(toHandle), pktHandleHeaderLen;
    char *pktHandleHeader, *message;
    int sndBufLen = strlen(send_buf), messageLen;

    pktHandleHeader = create_msg_handle_header(toHandle, &pktHandleHeaderLen);

    messageLen = sndBufLen - 4 - toHandleLen;
    message = send_buf + 4 + toHandleLen;
    send_msg_pkts(server_socket, 5, pktHandleHeader, pktHandleHeaderLen, message, messageLen);
    free(pktHandleHeader);
}

void send_brdcst_pkt(int server_socket, char *send_buf) {
    int myHandleLen = strlen(handle), pktHandleHeaderLen;
    char *pktHandleHeader, *message;
    int sndBufLen = strlen(send_buf), messageLen;

    pktHandleHeaderLen = myHandleLen + 1; // add 1 for length bit
    pktHandleHeader = malloc(pktHandleHeaderLen);
    pktHandleHeader[0] = (uint8_t) myHandleLen;
    memcpy(pktHandleHeader + 1, handle, myHandleLen);

    messageLen = sndBufLen - 3;
    message = send_buf + 3;
    send_msg_pkts(server_socket, 4, pktHandleHeader, pktHandleHeaderLen, message, messageLen);
    free(pktHandleHeader);
    printf("$: ");        // no ack for broadcast so print again
    fflush(stdout);
}

char *create_msg_handle_header(char *toHandle, int *pktHandleHeaderLen) {
    int toHandleLen = strlen(toHandle), myHandleLen = strlen(handle);
    char *pktHandleHeader;

    *pktHandleHeaderLen = myHandleLen + 1 + toHandleLen + 1;    
    pktHandleHeader = malloc(*pktHandleHeaderLen);
    pktHandleHeader[0] = (uint8_t) toHandleLen;
    memcpy(pktHandleHeader + 1, toHandle, toHandleLen);
    pktHandleHeader[toHandleLen + 1] = (uint8_t) myHandleLen;
    memcpy(pktHandleHeader + 2 + toHandleLen, handle, myHandleLen);
    return pktHandleHeader;
}

void create_and_send_msg(char *send_buf, int server_socket) {
    char *send_buf_dup = strdup(send_buf);
    char *token;
    char *toHandle;
    char *pktHandleHeader;
    char *delim = " ";
    char *pkt = NULL;
    int pktHandleHeaderLen;
    

    token = strtok(send_buf_dup, delim);

    if (token != NULL && (strcmp(token, "%M") == 0 || strcmp(token, "%m") == 0)) { // message to a client
        toHandle = strtok(NULL, delim);
        if (toHandle == NULL) {
            printf("Error, no handle given");
            fflush(stdout);
            return;
        }
        if(strtok(NULL, delim) == NULL) {  // no msg so send empty message packet
            printf("toHandle is %s\n", toHandle);
            pktHandleHeader = create_msg_handle_header(toHandle, &pktHandleHeaderLen);
            pkt = create_full_packet((uint8_t)5, pktHandleHeader, pktHandleHeaderLen);
            tcp_send(server_socket, pkt, NRML_HDR_LEN + pktHandleHeaderLen);
            free(pkt);
        } else {
            send_msg_pkt(server_socket, send_buf, toHandle);
        }
    } else if (token != NULL && (strcmp(token, "%B") == 0 || strcmp(token, "%b") == 0)) { //broadcast msg
        send_brdcst_pkt(server_socket, send_buf);
    } else if (token != NULL && (strcmp(token, "%L") == 0 || strcmp(token, "%l") == 0)) { // list handles
        pkt = create_full_packet((uint8_t) 10, NULL, 0);
        tcp_send(server_socket, pkt, NRML_HDR_LEN);
        free(pkt);
        return;
    } else if (token != NULL && (strcmp(token, "%E") == 0 || strcmp(token, "%e") == 0)) { // exit
        pkt = create_full_packet((uint8_t) 8, NULL, 0);
        tcp_send(server_socket, pkt, NRML_HDR_LEN);
        free(pkt);
        return;
    }
    else {
        printf("Invalid command %s.\n", token);
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
    int bufsize;         //data buffer size
    int send_len= 0;        //amount of data to send

    /* initialize data buffer for the packet */
    bufsize= 8 * MAX_PKT_LEN;   // allow 8 times the 64 kb of data
    send_buf = (char *) malloc(bufsize);
    memset(send_buf, 0, bufsize);

    send_len = 0;
    while ((send_buf[send_len] = getchar()) != '\n') {
        send_len++;
    }
    //fgets(send_buf, bufsize, stdin);

    send_buf[send_len] = '\0';


    /* initialize pkt and add it to send_buf */
    create_and_send_msg(send_buf, server_socket);
    free(send_buf);
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
        if (select(server_socket + 1, &read_sockets, NULL, NULL, NULL) < 0) {
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
    printf("\n%s: %s\n", fromHandle, msg);
}

void get_message(char *pkt, int client_socket) {
    uint8_t *toHandleLen, *fromHandleLen;
    char *msg;
    //char *toHandle, *fromHandle;

    toHandleLen = (uint8_t *)pkt;
    fromHandleLen = (uint8_t *)(pkt + 1 + *toHandleLen);
    msg = pkt + *toHandleLen + *fromHandleLen + 2;
    //printf("toHandleLen %u fromHandleLen %u\n", *toHandleLen, *fromHandleLen);
    printf("\n%s: %s\n", get_handle_name(pkt + *toHandleLen + 2, *fromHandleLen), msg);
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
        free(curHandle);
    }
    printf("\n");
}

int get_pkt(int socket) {
    int message_len = 0;     //length of the received message
    char *buf;              //buffer for receiving from client
    int buffer_size= MAX_PKT_LEN;  //packet size variable
    struct nrml_hdr pkt;
    int knownHandles;

    //create packet buffer
    buf = (char *) malloc(buffer_size);
    memset(buf, 0, buffer_size);
    //now get the data on the socket
    if ((message_len = recv(socket, buf, buffer_size, 0)) < 0)
    {
        perror("recv call");
        exit(-1);
    } else if (message_len == 0) {   // server exitted
        close(socket);
        printf("Server Terminated\n");
        free(buf);
        return socket;     
    }
    //printf("recieved %d bytes\n", message_len);
    //print_packet(buf, message_len);

    
    memcpy(&pkt, buf, 7);
    //printf("Recieved packet: seqNum = %d, len = %hu, flag = %hhu\n", ntohs(pkt.seqNum), ntohs(pkt.len), ntohs(pkt.flag) >> 8);
    //print_packet(buf, message_len);
    switch (pkt.flag) {
    case 2:
        //printf("Welcome %s\n", handle);
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
        free(buf);
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

    free(buf);
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
    free(pktTail);

    sent =  send(socket_num, fullPkt, fullPktLen, 0);
    if(sent < 0)
    {
        perror("send call");
        exit(-1);
    }

    free(fullPkt);

    // wait for ack
    get_pkt(socket_num);

    return socket_num;
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
