#ifndef RCOPY_H
#define RCOPY_H

#include "networks.h"

void sendPacket(Connection server, Packet packet);
Connection udp_send_setup(char *host_name, char *port);
void print_packet(void * start, int bytes);
state sendFilename(Connection server, char *localFile, char *remoteFile);

#endif
