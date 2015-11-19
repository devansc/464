#ifndef RCOPY_H
#define RCOPY_H

#include "networks.h"

Connection udp_send_setup(char *host_name, char *port);
STATE sendFilename(char *localFile, char *remoteFile);
STATE sendWindowSize(int window);
STATE stopAndWait(Packet packet, int numTriesLeft, int flagExpected, STATE nextState);
STATE sendData(Packet *filePackets);
STATE recieveAcks();

#endif
