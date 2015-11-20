#ifndef RCOPY_H
#define RCOPY_H

#include "networks.h"

/*
enum STATE {
    FILENAME, WINDOW, DATA, ACK, WAIING_ACK, EOFCONFIRM, DONE
};
*/

Connection udp_send_setup(char *host_name, char *port);
STATE sendFilename(char *localFile, char *remoteFile);
STATE sendWindowSize();
STATE stopAndWait(Packet packet, int numTriesLeft, int flagExpected, STATE nextState);
STATE sendData(Packet *filePackets);
STATE recieveAcks(Packet *filePackets);
int getRRSeqNum(Packet ackPacket);

#endif
