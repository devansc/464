#ifndef NETWORKS_H
#define NETWORKS_H
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_LEN_PKT 1100
#define HDR_LEN 9

#define FLAG_DATA 1
#define FLAG_RESEND 2
#define FLAG_RR 3
#define FLAG_SREJ 4
#define FLAG_FILENAME 6
#define FLAG_WINDOW 7

enum SELECTVAL {
    SELECT_HAS_DATA, SELECT_TIMEOUT
};

enum STATE {
    FILENAME, WINDOW, DATA, ACK, EOFCONFIRM, DONE
};

struct connection {
    int socket;
    struct sockaddr_in address;
    size_t addr_len;
};
typedef struct connection Connection;

struct packet {
    uint32_t seq_num;
    int16_t checksum;
    int8_t flag;
    uint16_t size;
    char *payload;
    char *data;
};
typedef struct packet Packet;


Packet createPacket(uint32_t seq_num, int flag, char *payload, int size_payload);
Packet fromPayload(char *payload);
char *getData(Packet p);
Packet recievePacket(Connection *connection);
SELECTVAL selectCall(int socket, int timeoutSec);
void sendPacket(Connection server, Packet packet);
void print_packet(void * start, int bytes);

#endif
