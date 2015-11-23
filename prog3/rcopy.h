#ifndef RCOPY_H
#define RCOPY_H

#include "networks.h"

Connection udp_send_setup(char *host_name, char *port);
Packet *createFilePackets(char *fileName);
STATE stopAndWait(Packet packet, int numTriesLeft, int flagExpected, STATE nextState);
STATE sendData();
STATE recieveAcks();
STATE sendEOF();
STATE getRestAcks();
int getRRSeqNum(Packet ackPacket);
int openFile(char *filename);
void sendDone();
Packet *createFilePackets(char *filename, int sizeFile);
void checkAndGetArgs(int argc, char *argv[]);

#endif
