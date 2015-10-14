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
    int socket_num;         //socket descriptor
    seq_num = 16;

    /* check command line arguments  */
    if(argc!= 4)
    {
        printf("usage: %s host-name port-number \n", argv[0]);
        exit(1);
    }

    /* set up the socket for TCP transmission  */
    handle = strdup(argv[1]);
    socket_num = tcp_send_setup(argv[2], argv[3]);

    while (1) {
        tcp_send_message(socket_num);
    }

        
    close(socket_num);
    return 0;
}

char *create_msg_hdr(char *send_buf) {
    char *send_buf_dup = strdup(send_buf);
    char *token;
    char *delim = " ";
    char *hdr = NULL;
    char *toHandle;
    int toHandleLen, myHandleLen = strlen(handle);
    int totalMsgLen = strlen(send_buf) + myHandleLen + 1;
    
    token = strtok(send_buf_dup, delim);

    if (token != NULL && strcmp(token, "%M") == 0) {
        toHandle = strtok(NULL, delim);
        toHandleLen = strlen(toHandle);
        totalMsgLen += toHandleLen + 1;    
        totalMsgLen -= 4 + toHandleLen;        // subtract the %M [handle] from totalMsgLen
        //printf("totalMsgLen %d\n", totalMsgLen);
        //printf("handle is %s\n", toHandle);
        hdr = malloc(totalMsgLen);
        hdr[0] = (uint8_t) toHandleLen;
        memcpy(hdr + 1, toHandle, toHandleLen);
        hdr[toHandleLen + 1] = (uint8_t) myHandleLen;
        memcpy(hdr + 2 + toHandleLen, handle, myHandleLen);
        //printf("trying to put message %s\n", send_buf + 4 + toHandleLen);
        memcpy(hdr + 2 + toHandleLen + myHandleLen, send_buf + 4 + toHandleLen, strlen(send_buf) - 4 - toHandleLen);
        hdr[totalMsgLen] = 0;
        print_packet(hdr, totalMsgLen);
    }
    else {
        printf("Invalid command\n");
    }
    return hdr;
}

void tcp_send_message(int socket_num) {
    char *send_buf;         //data buffer
    int bufsize= 0;         //data buffer size
    int send_len= 0;        //amount of data to send
    int sent= 0;            //actual amount of data sent
    char *pkt;
    char *msgHdr;

    /* initialize data buffer for the packet */
    bufsize= 1024;
    send_buf = (char *) malloc(bufsize);

    printf("Enter the data to send: ");
    
    send_len = 0;
    while ((send_buf[send_len] = getchar()) != '\n' && send_len < 80)
	   send_len++;

    send_buf[send_len] = '\0';

    /* initialize pkt and add it to send_buf */
    msgHdr = create_msg_hdr(send_buf);
    pkt = create_full_packet((uint8_t)5, msgHdr, strlen(msgHdr));
            
    /* now send the data */
    sent =  send(socket_num, pkt, NRML_HDR_LEN + strlen(msgHdr), 0);
    if(sent < 0)
    {
        perror("send call");
        exit(-1);
    }

    //printf("String sent: %s \n", msgHdr);
    //printf("Amount of data sent is: %d\n", sent);
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
    pkt.flag = htons(flag) >> 8;
    fullPkt = malloc(len);
    memcpy(fullPkt, &pkt, NRML_HDR_LEN);
    memcpy(fullPkt + NRML_HDR_LEN, pktTail, pktTailLen);
    //printf("created packet ");
    //print_packet(fullPkt, len);
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
