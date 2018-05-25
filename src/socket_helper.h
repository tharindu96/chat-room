#ifndef _SOCKET_HELPER_H
#define _SOCKET_HELPER_H 1

int socket_helper_tcp_listen(char *local_port);
int socket_helper_tcp_connect(char *remote_host, char *remote_port);

int socket_helper_sendall(int sockfd, void *buffer, int len);
int socket_helper_recvall(int sockfd, void **buffer, int *len);

int socket_helper_accept(int sockfd, int **sockets);

#endif // _SOCKET_HELPER_H
