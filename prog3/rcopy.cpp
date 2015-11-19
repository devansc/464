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

int seq_num = 0;
Connection connection;   
int bottom;
int lower;
int upper;
int totalPackets;

int main(int argc, char *argv[]) {
    STATE curState;
    Packet packet; // for testing
    char packetData[] = "1";
    Packet *filePackets; // for testing

    if (argc != 6) {
        printf("usage %s [local-file] [remote-file] [window-size] [remote-machine] [remote-port]\n", argv[0]);
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
            curState = sendWindowSize(atoi(argv[3]));
            break;

        case DATA:
            totalPackets = 1;
            packet = createPacket(seq_num++, FLAG_DATA, packetData, 2);
            filePackets = &packet;
            curState = sendData(filePackets);
            break;

        case ACK:
            curState = recieveAcks();
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

STATE recieveAcks() {
    //Packet ackPacket;
    while (selectCall(connection.socket, 0) != SELECT_TIMEOUT) {
        //ackPacket =
    }
    return DONE;
}

STATE sendData(Packet *filePackets) {
    if (lower < upper) {
        sendPacket(connection, *filePackets);
        filePackets++;
        lower++;
    } 

    if (selectCall(connection.socket, 0) == SELECT_TIMEOUT) {
        if (lower < upper) 
            return sendData(filePackets);
        else // have to select 1 sec for acks
            printf("have to select 1 sec for acks\n");
    } else {
        return ACK;
    }
    printf("WARNING should never get here, exitting\n");
    return DONE; 
}



STATE sendWindowSize(int window) {
    Packet packet;

    bottom = 0;
    lower = 0;
    upper = window;

    printf("sending window %d\n", window);
    window = htonl(window);
    packet = createPacket(seq_num++, FLAG_WINDOW, (char *)&window, sizeof(int));
    print_packet(packet.data, packet.size - HDR_LEN);
    return stopAndWait(packet, 10, FLAG_WINDOW_ACK, DONE);
}

STATE sendFilename(char *localFile, char *remoteFile) {
    Packet packet;
    FILE *transferFile;

    transferFile = fopen(localFile, "r");
    if (transferFile == NULL) {
        fprintf(stderr, "File %s does not exist, exitting", localFile);
        exit(1);
    }

    printf("sending filename %s to port %d\n", remoteFile, ntohs(connection.address.sin_port));
    packet = createPacket(seq_num++, FLAG_FILENAME, remoteFile, strlen(remoteFile));
    return stopAndWait(packet, 10, FLAG_FILENAME_ACK, WINDOW);
}

STATE stopAndWait(Packet packet, int numTriesLeft, int flagExpected, STATE nextState) {
    Packet ackPacket;

    if (numTriesLeft <= 0) {
        printf("Server disconnected\n");
        return DONE;
    }

    sendPacket(connection, packet);
    if (selectCall(connection.socket, DEFAULT_TIMEOUT) == SELECT_TIMEOUT) {
        printf("timed out waiting for server ack");
        return stopAndWait(packet, numTriesLeft - 1, flagExpected, nextState);
    }
    ackPacket = recievePacket(&connection);
    if (ackPacket.flag == flagExpected) {
        printf("done in %d tries\n", 10 - numTriesLeft);
        return nextState;
    }
    return stopAndWait(packet, numTriesLeft, flagExpected, nextState);  // will send a duplicate packet that could be unnecessary
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

