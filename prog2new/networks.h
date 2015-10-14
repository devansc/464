// for the server side
int tcp_recv_setup();
int tcp_listen(int server_socket, int back_log);
void get_pkt(int client_socket);
void get_message(char *pkt, int client_socket);
void get_handle(char *pkt, int pkt_len, int client_socket);
char *get_handle_name(char *handleStart, uint8_t handleLen);
void send_recieve_packets(int server_socket);

// for the client side
int tcp_send_setup(char *host_name, char *port);
void tcp_send_message(int socket_num);
void print_packet(void * start, int bytes);
char *create_full_packet(uint8_t flag, char *pktTail, int pktTailLen);

