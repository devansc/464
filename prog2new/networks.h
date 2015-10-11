// for the server side
int tcp_recv_setup();
int tcp_listen(int server_socket, int back_log);
void get_pkt(int client_socket);
void get_message(char *pkt, int pkt_len);
void get_handle(char *pkt, int pkt_len);

// for the client side
int tcp_send_setup(char *handle, char *host_name, char *port);
void tcp_send_message(int socket_num);
void print_packet(void * start, int bytes);
char *create_full_packet(uint8_t flag, char *pktTail, int pktTailLen);

