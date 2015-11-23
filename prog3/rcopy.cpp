#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <math.h>

#include "networks.h"
#include "rcopy.h"
#include "cpe464.h"

#define DEFAULT_TIMEOUT 1
#define NUM_EST_PCKTS 2

uint32_t seq_num;
Connection connection;   
uint32_t bottomWindow;
uint32_t lowerWindow;
uint32_t upperWindow;
FILE *transferFile;
uint32_t totalFilePackets;
uint32_t windowSize;

int main(int argc, char *argv[]) {
    STATE curState;
    Packet *filePackets; // for testing
    int sizeFile;

    if (argc != 6) {
        printf("usage %s [local-file] [remote-file] [window-size] [remote-machine] [remote-port]\n", argv[0]);
        exit(-1);
    }

    windowSize = (uint32_t) atoi(argv[3]);
    if (windowSize < 0) {
        fprintf(stderr, "Error window size must be greater than 0");
        exit(-1);
    }
    sendErr_init(.15, DROP_ON, FLIP_OFF, DEBUG_ON, RSEED_OFF);
    // 4 is remote-machine, 5 is remote-port
    connection = udp_send_setup(argv[4], argv[5]);

    seq_num = 0;
    sizeFile = openFile(argv[1]);
    filePackets = createFilePackets(argv[2], sizeFile); 
    
    //printf("connection socket %d, address %s\n", connection.socket, inet_ntoa(connection.address->sin_addr));

    curState = FILENAME;
    
    while (curState != DONE) {
        switch (curState) {

        case FILENAME:
            curState =  stopAndWait(filePackets[0], 10, filePackets[0].seq_num + 1, WINDOW);
            break;

        case WINDOW:
            lowerWindow = bottomWindow = 2;
            upperWindow = windowSize + 2;
            curState =  stopAndWait(filePackets[1], 10, filePackets[1].seq_num + 1, DATA);
            break;

        case DATA:
            curState = sendData(filePackets);
            break;

        case ACK:
            curState = recieveAcks(filePackets);
            break;

        case EOFCONFIRM:
            curState = sendEOF();
            sendDone();
            break;

        case DONE:
            break;
        }
    }

    printf("done\n");
    return 0;
}

void sendDone() {
    Packet eofPacket = createPacket(seq_num++, FLAG_QUIT, NULL, 0);
    printf("sending done packet\n");
    sendPacket(connection, eofPacket);
}

//returns number of bytes of file
int openFile(char *filename) {
    long size;
    transferFile = fopen(filename, "r");
    if (transferFile == NULL) {
        fprintf(stderr, "File %s does not exist, exitting", filename);
        exit(1);
    }
    fseek(transferFile, 0L, SEEK_END);
    size = ftell(transferFile);
    printf(" SIZE IS %ld\n", size);
    fseek(transferFile, 0L, SEEK_SET);
    return (int)size;
}

Packet *createFilePackets(char *filename, int sizeFile) {
    Packet pkt;
    int buf_size = 10;
    unsigned char buffer[buf_size];
    int bytes_read = 0;
    Packet filenamePacket;
    Packet windowPacket;
    int netWindow;
    size_t packetSize = sizeof(Packet);
    Packet *filePackets;
   
    totalFilePackets = ceil(((float) sizeFile) / buf_size) + 2;
    filePackets = (Packet *)malloc(sizeof(Packet) * totalFilePackets);

    // add filename and window packets
    filenamePacket = createPacket(seq_num, FLAG_FILENAME, (unsigned char *) filename, strlen(filename));
    memcpy(filePackets + seq_num++, &filenamePacket, packetSize);
    netWindow = htonl(windowSize);
    windowPacket = createPacket(seq_num, FLAG_WINDOW, (unsigned char *)&netWindow, sizeof(int));
    memcpy(filePackets + seq_num++, &windowPacket, packetSize);

    //process file
    while ((bytes_read = fread(buffer, sizeof(unsigned char), buf_size, transferFile)) > 0) {
        pkt = createPacket(seq_num, FLAG_DATA, (unsigned char *)buffer, bytes_read);
        memcpy(filePackets + seq_num++, &pkt, packetSize);
    }
    return filePackets;
}

STATE recieveAcks(Packet *filePackets) {
    Packet ackPacket;
    uint32_t rrNum;
    int foundAcks = 0;

    while (selectCall(connection.socket, 0) != SELECT_TIMEOUT) {
        foundAcks = 1;
        ackPacket = recievePacket(&connection);

        //TODO run checksum

        rrNum = getRRSeqNum(ackPacket);
        bottomWindow = rrNum;
        upperWindow = rrNum + windowSize;
        if (ackPacket.flag == FLAG_SREJ) {
            printf("recieved srej %d so setting bottom to %d, lowerWindow %d, upper %d\n", rrNum, bottomWindow, lowerWindow, upperWindow);
            sendPacket(connection, filePackets[getRRSeqNum(ackPacket)]);
            return ACK;
        } 
        printf("recieved rrnum %d so setting bottom to %d, lowerWindow %d, upper %d\n", rrNum, rrNum, lowerWindow, rrNum + windowSize);
    }
    if (foundAcks || lowerWindow < upperWindow) // if lower < upper keep sending data
        return DATA;
    
    // if got to here, then need to wait for 1 sec on acks, then send 
    // bottom of window if it times out. if select finds data, process ack
    if (selectCall(connection.socket, 1) == SELECT_TIMEOUT) { 
        printf("WARNING sending bottom of window %d\n", bottomWindow);
        sendPacket(connection, filePackets[bottomWindow]);
    } 
    return ACK;
}

STATE sendData(Packet *filePackets) {
    printf("running send data, bottom = %d, lower = %d, upper = %d, total file packets %d\n", bottomWindow, lowerWindow, upperWindow, totalFilePackets);
    if (lowerWindow < upperWindow && lowerWindow < totalFilePackets) {
        sendPacket(connection, filePackets[lowerWindow]);
        printf("sent data %s\n", filePackets[lowerWindow].data);
        lowerWindow++;
    } else if (lowerWindow == totalFilePackets) {
        printf("done sending data, sending eof\n");
        return EOFCONFIRM;
    }

    if (selectCall(connection.socket, 0) == SELECT_TIMEOUT && lowerWindow < upperWindow)  {
        printf("select didn't find data, going to send another packet\n");
        return DATA;
    }
    else // acks to process, or have to have to select 1 sec for acks
        printf("select immediately found data or waiting on acks\n");
        return ACK;
}

STATE sendEOF() {
    Packet EOFPacket = createPacket(seq_num++, FLAG_EOF, NULL, 0);
    return stopAndWait(EOFPacket, 10, EOFPacket.seq_num + 1, DONE);
}

STATE stopAndWait(Packet packet, int numTriesLeft, int rrExpected, STATE nextState) {
    Packet ackPacket;

    if (numTriesLeft <= 0) {
        printf("Server disconnected\n");
        return DONE;
    }

    sendPacket(connection, packet);
    if (selectCall(connection.socket, DEFAULT_TIMEOUT) == SELECT_TIMEOUT) {
        printf("timed out waiting for server ack");
        return stopAndWait(packet, numTriesLeft - 1, rrExpected, nextState);
    }
    ackPacket = recievePacket(&connection);
    printf("recieved rr for %d\n", getRRSeqNum(ackPacket));
    //printf("packet is ");
    //print_packet(ackPacket.data, ackPacket.size-HDR_LEN -1);
    if (getRRSeqNum(ackPacket) == rrExpected) {
        //printf("done in %d tries\n", 10 - numTriesLeft);
        return nextState;
    }
    return stopAndWait(packet, numTriesLeft, rrExpected, nextState);  // will send a duplicate packet that could be unnecessary
}

int getRRSeqNum(Packet ackPacket) {
    int *rrNum = (int*)ackPacket.data;
    return ntohl(*rrNum);
}

Connection udp_send_setup(char *host_name, char *port) {
    Connection newConnection;
    int socket_num;
    struct sockaddr_in remote;       // socket address for remote side
    struct hostent *hp;              // address of remote host
    int sockaddrinSize = sizeof(struct sockaddr_in);

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
    newConnection.addr_len = sockaddrinSize;
    //printf("created connection socket %d, address %s\n", socket_num, inet_ntoa(remote.sin_addr));

    return newConnection;
}

