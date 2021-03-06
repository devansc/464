#ifndef SERVER_H
#define SERVER_H

#include "networks.h"

int udp_recv_setup(int port);
void initFields(Packet *pkt);
char *pktToString(Packet packet);
void processClient(int socket);
STATE recieveFilename();
STATE recieveWindow();
STATE recieveData();
STATE eofConfirmQuit();
void sendResponse(int flag, int rrNum);

#endif
