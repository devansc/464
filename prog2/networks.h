#define NRML_HDR_LEN 7
//#define MAX_PKT_LEN 0x10000
//#define MAX_PKT_LEN 64
#define MAX_PKT_LEN 65536

#define INIT_MSG 1
#define INIT_ACK 2
#define INIT_ACK_ERR 3
#define BRDCST_MSG 4
#define REG_MSG 5
#define MSG_ACK 6
#define MSG_ACK_ERR 7
#define EXIT_MSG 8
#define EXIT_ACK 9
#define HNDL_REQUEST 10
#define HNDL_NUM 11
#define HNDL_PKT 12


// for the server side
int tcp_recv_setup();
int tcp_listen(int server_socket, int back_log);
int get_pkt(int client_socket);
void get_message(char *pkt, int client_socket);
void get_handle(char *pkt, int pkt_len, int client_socket);
char *get_handle_name(char *handleStart, uint8_t handleLen);
void send_recieve_packets(int listening_socket);
int get_handle_socket(char *handle);
void forward_broadcast_message(char *pktStart, int client_socket);
void close_socket(int socket);

// for the client side
int tcp_send_setup(char *host_name, char *port);
void tcp_send_message(int server_socket);
void tcp_send_recv(int server_socket);
void print_packet(void * start, int bytes);
char *create_full_packet(uint8_t flag, char *pktTail, int pktTailLen);
void create_and_send_msg(char *send_buf, int server_socket);
void recieve_handle_packet(uint8_t *handles, int totalLen);
void send_brdcst_pkt(int server_socket, char *send_buf);
void send_msg_pkt(int server_socket, char *send_buf, char *toHandle);
char *create_msg_handle_header(char *toHandle, int *pktHandleHeaderLen);

// shared
void tcp_send(int socket, char *data, int len_data);
