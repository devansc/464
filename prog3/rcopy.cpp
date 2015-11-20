#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "networks.h"
#include "rcopy.h"
#include "cpe464.h"

#define DEFAULT_TIMEOUT 1
#define NUM_EST_PCKTS 2

int seq_num = 0;
Connection connection;   
uint32_t bottomWindow;
uint32_t lowerWindow;
uint32_t upperWindow;
FILE *transferFile;
uint32_t totalFilePackets;
uint32_t windowSize;

int main(int argc, char *argv[]) {
    STATE curState;
    Packet packet; // for testing
    Packet packet1; // for testing
    Packet packet2; // for testing
    char *packetData[] = {"hey there", " how are you", "\n\n:)"};
    Packet *filePackets; // for testing

    if (argc != 6) {
        printf("usage %s [local-file] [remote-file] [window-size] [remote-machine] [remote-port]\n", argv[0]);
        exit(-1);
    }

    windowSize = (uint32_t) atoi(argv[3]);
    if (windowSize < 0) {
        fprintf(stderr, "Error window size must be greater than 0");
        exit(-1);
    }
    sendErr_init(0, DROP_OFF, FLIP_OFF, DEBUG_ON, RSEED_OFF);
    // 4 is remote-machine, 5 is remote-port
    connection = udp_send_setup(argv[4], argv[5]);
    
    //printf("connection socket %d, address %s\n", connection.socket, inet_ntoa(connection.address->sin_addr));

    curState = FILENAME;
    
    while (curState != DONE) {
        switch (curState) {

        case FILENAME:
            curState = sendFilename(argv[1], argv[2]);
            break;

        case WINDOW:
            curState = sendWindowSize();
            break;

        case DATA:
            totalFilePackets = 3;
            packet = createPacket(seq_num++, FLAG_DATA, packetData[0], strlen(packetData[0]) + 1);
            packet1 = createPacket(seq_num++, FLAG_DATA, packetData[1], strlen(packetData[1]) + 1);
            packet2 = createPacket(seq_num++, FLAG_DATA, packetData[2], strlen(packetData[2]) + 1);
            filePackets = &packet;
            curState = sendData(filePackets);
            break;

        case ACK:
            curState = recieveAcks(filePackets);
            break;

        case EOFCONFIRM:
            break;

        case DONE:
            break;
        }
    }

    printf("done\n");
    return 0;
}

STATE recieveAcks(Packet *filePackets) {
    Packet ackPacket;
    uint32_t rrNum;
    int foundAcks = 0;

    while (selectCall(connection.socket, 0) != SELECT_TIMEOUT) {
        foundAcks = 1;
        ackPacket = recievePacket(&connection);

        //TODO run checksum

        rrNum = ackPacket.seq_num;
        printf("recieved rrnum %d so setting bottom to %d, lower %d, upper %d\n", rrNum, rrNum, lowerWindow, rrNum + windowSize);

        if (bottomWindow < rrNum) {
            bottomWindow = rrNum;
            upperWindow = rrNum + windowSize;
        }
    }
    if (foundAcks || lowerWindow < upperWindow) // if lower < upper keep sending data
        return DATA;
    
    // if got to here, then need to wait for 1 sec on acks, then send 
    // bottom of window if it times out. if select finds data, process ack
    if (selectCall(connection.socket, 1) == SELECT_TIMEOUT) { 
        printf("WARNING sending bottom of window\n");
        sendPacket(connection, filePackets[bottomWindow]);
    } 
    return ACK;
}

STATE sendData(Packet *filePackets) {
    if (lowerWindow < upperWindow && lowerWindow < totalFilePackets) {
        sendPacket(connection, *filePackets);
        printf("sent data %s\n", (*filePackets).data);
        filePackets++;
        lowerWindow++;
    } else if (lowerWindow == totalFilePackets) {
        printf("done sending data, exitting\n");
        return DONE;
    }

    if (selectCall(connection.socket, 0) == SELECT_TIMEOUT) {
        if (lowerWindow < upperWindow) 
            return sendData(filePackets);
        else // have to select 1 sec for acks
            return ACK;
    } else {  // theres acks to process
        return ACK;
    }
    printf("WARNING should never get here, exitting\n");
    return DONE; 
}



STATE sendWindowSize() {
    Packet packet;
    int netWindow;

    bottomWindow = 0;
    lowerWindow = 0;
    upperWindow = windowSize;

    //printf("sending window %d\n", windowSize);
    netWindow = htonl(windowSize);
    packet = createPacket(seq_num++, FLAG_WINDOW, (char *)&netWindow, sizeof(int));
    //print_packet(packet.data, packet.size - HDR_LEN);
    return stopAndWait(packet, 10, packet.seq_num + 1, DATA);
}

STATE sendFilename(char *localFile, char *remoteFile) {
    Packet packet;

    transferFile = fopen(localFile, "r");
    if (transferFile == NULL) {
        fprintf(stderr, "File %s does not exist, exitting", localFile);
        exit(1);
    }

    printf("sending filename %s to port %d\n", remoteFile, ntohs(connection.address.sin_port));
    packet = createPacket(seq_num++, FLAG_FILENAME, remoteFile, strlen(remoteFile));
    return stopAndWait(packet, 10, packet.seq_num + 1, WINDOW);
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
        printf("done in %d tries\n", 10 - numTriesLeft);
        return nextState;
    }
    return stopAndWait(packet, numTriesLeft, rrExpected, nextState);  // will send a duplicate packet that could be unnecessary
}

int getRRSeqNum(Packet ackPacket) {
    int *rrNum = (int*)ackPacket.data;
    printf("rrnum is %d\n", *rrNum);
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
    printf("created connection socket %d, address %s\n", socket_num, inet_ntoa(remote.sin_addr));

    return newConnection;
}

