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
uint32_t bufferSize;
float errorPercent;
Packet *filePackets;

int main(int argc, char *argv[]) {
    STATE curState;
    int sizeFile;

    checkAndGetArgs(argc, argv);

    sendErr_init(errorPercent, DROP_ON, FLIP_OFF, DEBUG_ON, RSEED_ON);
    // 4 is remote-machine, 5 is remote-port
    connection = udp_send_setup(argv[6], argv[7]);

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
            curState = sendData();
            break;

        case ACK:
            curState = recieveAcks();
            break;

        case LAST_ACKS:
            curState = getRestAcks();
            break;

        case EOFCONFIRM:
            printf("done sending data, sending eof\n");
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

void checkAndGetArgs(int argc, char *argv[]) {
    if (argc != 8) {
        printf("usage %s [local-file] [remote-file] [buffer-size] [error-percent] [window-size] [remote-machine] [remote-port]\n", argv[0]);
        exit(-1);
    }
    windowSize = (uint32_t) atoi(argv[5]);
    if (windowSize < 0) {
        fprintf(stderr, "Error window-size must be greater than 0");
        exit(-1);
    }
    bufferSize = (uint32_t) atoi(argv[3]);
    if (bufferSize < 0 || bufferSize > 1400) {
        fprintf(stderr, "Error buffer-size must be greater than 0, less than 1400");
        exit(-1);
    }
    errorPercent = atof(argv[4]);
    if (errorPercent < 0 || errorPercent > 1) {
        fprintf(stderr, "Error error-percent must be greater than 0, less than 1");
        exit(-1);
    }
}

STATE getRestAcks() {
    Packet recvPacket;

    if (selectCall(connection.socket, DEFAULT_TIMEOUT) == SELECT_TIMEOUT) {
        sendPacket(connection, filePackets[bottomWindow]);
        printf("timed out getting last acks\n");
        return getRestAcks();
    }
    
    recvPacket = recievePacket(&connection);
    uint32_t rrNum = getRRSeqNum(recvPacket);
    if (recvPacket.flag == FLAG_RR){
        bottomWindow = rrNum;
        upperWindow = rrNum + windowSize;
        printf("recieved rrnum %d\n", rrNum);
        if (rrNum == totalFilePackets)
            return EOFCONFIRM;
    } else {
        printf("recieved srej %d\n", rrNum);
        sendPacket(connection, filePackets[rrNum]);
        bottomWindow = rrNum;
        upperWindow = rrNum + windowSize;
        return DATA;
    }
    return getRestAcks();
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
    fseek(transferFile, 0L, SEEK_SET);
    return (int)size;
}

Packet *createFilePackets(char *filename, int sizeFile) {
    Packet pkt;
    unsigned char buffer[bufferSize];
    int bytes_read = 0;
    Packet filenamePacket;
    Packet windowPacket;
    int netWindow;
    size_t packetSize = sizeof(Packet);
    Packet *fp;
   
    totalFilePackets = ceil(((float) sizeFile) / bufferSize) + 2;
    fp = (Packet *)malloc(sizeof(Packet) * totalFilePackets);

    // add filename and window packets
    filenamePacket = createPacket(seq_num, FLAG_FILENAME, (unsigned char *) filename, strlen(filename));
    memcpy(fp + seq_num++, &filenamePacket, packetSize);
    netWindow = htonl(windowSize);
    windowPacket = createPacket(seq_num, FLAG_WINDOW, (unsigned char *)&netWindow, sizeof(int));
    memcpy(fp + seq_num++, &windowPacket, packetSize);

    //process file
    while ((bytes_read = fread(buffer, sizeof(unsigned char), bufferSize, transferFile)) > 0) {
        pkt = createPacket(seq_num, FLAG_DATA, (unsigned char *)buffer, bytes_read);
        memcpy(fp + seq_num++, &pkt, packetSize);
    }
    return fp;
}

STATE recieveAcks() {
    Packet ackPacket;
    uint32_t rrNum;
    //int foundAcks = 0;

    while (selectCall(connection.socket, 0) != SELECT_TIMEOUT) {
        //foundAcks = 1;
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
    if (/*foundAcks ||*/ lowerWindow < upperWindow) // if lower < upper keep sending data
        return DATA;
    
    // if got to here, then need to wait for 1 sec on acks, then send 
    // bottom of window if it times out. if select finds data, process ack
    if (selectCall(connection.socket, 1) == SELECT_TIMEOUT) { 
        printf("WARNING sending bottom of window %d\n", bottomWindow);
        sendPacket(connection, filePackets[bottomWindow]);
    } 
    return ACK;
}

STATE sendData() {
    printf("running send data, bottom = %d, lower = %d, upper = %d, total file packets %d\n", bottomWindow, lowerWindow, upperWindow, totalFilePackets);
    if (lowerWindow < upperWindow && lowerWindow < totalFilePackets) {
        sendPacket(connection, filePackets[lowerWindow]);
        printf("sent data %s\n", filePackets[lowerWindow].data);
        lowerWindow++;
    } else if (lowerWindow == totalFilePackets) {
        return LAST_ACKS;
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
    if (ackPacket.flag == FLAG_ERR_REMOTE) {
        printf("Error during file open of remote-file on server.\n");
        return DONE;
    }
    if (getRRSeqNum(ackPacket) == rrExpected) {
        return nextState;
    } else if (getRRSeqNum(ackPacket) < rrExpected && ackPacket.flag == FLAG_SREJ) {
        sendPacket(connection, filePackets[getRRSeqNum(ackPacket)]);
    }
    return stopAndWait(packet, numTriesLeft, rrExpected, nextState);  
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

