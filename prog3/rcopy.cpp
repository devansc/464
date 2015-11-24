#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "networks.h"
#include "rcopy.h"
#include "cpe464.h"

#define DEFAULT_TIMEOUT 1
#define NUM_EST_PCKTS 2

uint32_t seq_num;
Connection connection;   
uint32_t bottomWindow;
uint32_t lowerWindow;
int transferFile;
uint32_t totalFilePackets;
uint32_t windowSize;
uint32_t bufferSize;
float errorPercent;
bool hitEOF;
Packet *bufferPackets;

int main(int argc, char *argv[]) {
    STATE curState;
    int netWindow;
    Packet filenamePacket, windowPacket;

    checkAndGetArgs(argc, argv);

    sendErr_init(errorPercent, DROP_ON, FLIP_OFF, DEBUG_ON, RSEED_OFF);
    // 4 is remote-machine, 5 is remote-port
    connection = udp_send_setup(argv[6], argv[7]);

    seq_num = 0;
    if((transferFile = open(argv[1], O_RDONLY)) < 0) {
        fprintf(stderr, "File %s does not exist, exitting", argv[1]);
        exit(1);
    }

    //filePackets = createFilePackets(argv[2], sizeFile); 
    
    //printf("connection socket %d, address %s\n", connection.socket, inet_ntoa(connection.address->sin_addr));

    curState = FILENAME;
    hitEOF = false;
    
    while (curState != DONE) {
        switch (curState) {

        case FILENAME:
            printf("sending filename %s\n", argv[2]);
            filenamePacket = createPacket(seq_num++, FLAG_FILENAME, (unsigned char *)argv[2], strlen(argv[2]) + 1);
            curState = stopAndWait(filenamePacket, 10, filenamePacket.seq_num + 1, WINDOW);
            break;

        case WINDOW:
            netWindow = htonl(windowSize);
            windowPacket = createPacket(seq_num++, FLAG_WINDOW, (unsigned char *)&netWindow, sizeof(int));
            lowerWindow = bottomWindow = 2;
            curState =  stopAndWait(windowPacket, 10, windowPacket.seq_num + 1, DATA);
            bufferPackets = (Packet *) malloc(sizeof(Packet) * windowSize);
            memset(bufferPackets, 0, sizeof(Packet) * windowSize);
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
    printf("running getRestAcks bottom %d lower %d\n", bottomWindow, lowerWindow);
    if (bottomWindow == lowerWindow)
        return EOFCONFIRM;

    if (selectCall(connection.socket, DEFAULT_TIMEOUT) == SELECT_TIMEOUT) {
        sendPacket(connection, bufferPackets[0]);
        printf("timed out getting last acks, sending packet %d\n", bufferPackets[0].seq_num);
        return getRestAcks();
    }
    
    recvPacket = recievePacket(&connection);
    uint32_t rrNum = getRRSeqNum(recvPacket);
    if (recvPacket.flag == FLAG_RR){
        bottomWindow = rrNum;
    } else {
        printf("recieved srej %d\n", rrNum);
        printf("rrnum %d bottomWindow %d windowSize %d\n", rrNum, bottomWindow, windowSize);
        printf("resending packet at pos %d, %d\n", (rrNum - bottomWindow) % windowSize, bufferPackets[(rrNum - bottomWindow) % windowSize].seq_num);
        sendPacket(connection, bufferPackets[(rrNum - bottomWindow) % windowSize]);
        bottomWindow = rrNum;
        return DATA;
    }
    return getRestAcks();
}


void sendDone() {
    Packet eofPacket = createPacket(seq_num++, FLAG_QUIT, NULL, 0);
    printf("sending done packet\n");
    sendPacket(connection, eofPacket);
}

void printBuffer() {
    Packet *curPkt;
    printf("printing buffer\n");
    for (int i = 0; i < windowSize; i++) {
        printf("  %d  ", i);
    }
    printf("\n");
    for (int i = 0; i < windowSize; i++) {
        printf(" %d  ", bufferPackets[i].seq_num);
    }
    printf("\n");
}

void slideWindow(int num) {
    int i;
    // could be simplified to 1 memcpy/memset
    printf("sliding window %d\n", num);
    for (i = 0; i < windowSize - num; i++) {
        memcpy(bufferPackets+i,bufferPackets+i+num,sizeof(Packet));
    }
    for (; i < windowSize; i++) {   
        memset(bufferPackets+i, 0, sizeof(Packet));
    }
    printBuffer();
}

STATE recieveAcks() {
    Packet ackPacket;
    uint32_t rrNum;
    int bufferPos;

    while (selectCall(connection.socket, 0) != SELECT_TIMEOUT) {
        ackPacket = recievePacket(&connection);

        //TODO run checksum

        rrNum = getRRSeqNum(ackPacket);
        printf("recieved rrnum %d so setting bottom to %d, lowerWindow %d, upper %d\n", rrNum, rrNum, lowerWindow, rrNum + windowSize);
        if (bottomWindow < rrNum) {
            slideWindow(rrNum - bottomWindow);
            bottomWindow = rrNum;
        }
        bufferPos = (rrNum - bottomWindow) % windowSize;

        if (ackPacket.flag == FLAG_SREJ && !hitEOF) {
            printf("recieved srej %d so setting bottom to %d, lowerWindow %d", rrNum, bottomWindow, lowerWindow);
            printf("resending packet at pos %d, %d\n", bufferPos, bufferPackets[bufferPos].seq_num);
            sendPacket(connection, bufferPackets[bufferPos]);
            bottomWindow = rrNum;
            return ACK;
        } 
    }
    if (lowerWindow < bottomWindow + windowSize)
        return DATA;
    
    
    // if got to here, then need to wait for 1 sec on acks, then send 
    // bottom of window if it times out. if select finds data, process ack
    if (selectCall(connection.socket, 1) == SELECT_TIMEOUT) { 
        printf("WARNING sending bottom of window %d\n", bottomWindow);
        sendPacket(connection, bufferPackets[0]);
    } 
    return ACK;
}

STATE sendData() {
    unsigned char buffer[bufferSize];
    int bufSpot = (seq_num - bottomWindow) % windowSize;
    int len_read = 0;
    int buffer_pos;
    Packet pkt;

    printf("running send data, bottom = %d, lower = %d, total file packets %d\n", bottomWindow, lowerWindow, totalFilePackets);
    if (hitEOF && bottomWindow < lowerWindow) {
        sendPacket(connection, bufferPackets[0]);
        return ACK;
    } else if (hitEOF && bottomWindow == lowerWindow) {
        return EOFCONFIRM;
    }


    len_read = read(transferFile, buffer, bufferSize);

    switch(len_read) {
    case -1:
        perror("send_data, read error");
        return DONE;
    case 0:
        printf("done sending data\n");
        hitEOF = true;
        return ACK;
    default:
        printf("creating packet size %d\n", len_read);
        pkt = createPacket(seq_num, FLAG_DATA, (unsigned char *)buffer, len_read);
        printf("sending data %d %s\n", pkt.seq_num, pkt.data);
        sendPacket(connection, pkt);
        memcpy(bufferPackets+bufSpot, &pkt, sizeof(Packet));
        printBuffer();
        seq_num++;
        lowerWindow = seq_num;
        printf("setting buffer[%d] to seq_num %d\n", bufSpot, pkt.seq_num);
        break;
    }

    if (selectCall(connection.socket, 0) == SELECT_TIMEOUT && lowerWindow < bottomWindow + windowSize)  {
        printf("select didn't find data, going to send another packet\n");
        return DATA;
    }
    else { // acks to process, or have to have to select 1 sec for acks
        printf("select immediately found data or waiting on acks\n");
        return ACK;
    }
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

