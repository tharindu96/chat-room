#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "socket_helper.h"

#define SOCKET_HELPER_BACKLOG 10
#define SOCKET_HELPER_BUFFER_SIZE_INIT 512
#define SOCKET_HELPER_BUFFER_SIZE_INCR 512

int socket_helper_tcp_listen(char *local_port)
{
	struct addrinfo hints, *res, *p;
	int status, yes = 1, sockfd = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; // ipv4 or ipv6
	hints.ai_socktype = SOCK_STREAM; // tcp connection
	hints.ai_flags = AI_PASSIVE; // fill my ip

	if ((status = getaddrinfo(NULL, local_port, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		return -1;
	}

	for (p = res; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("socket");
			continue;
		}
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
			perror("setsockopt");
			close(sockfd);
			sockfd = -1;
	     		continue;
		}
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			perror("bind");
			close(sockfd);
			sockfd = -1;
			continue;
		}
		if (listen(sockfd, SOCKET_HELPER_BACKLOG) == -1) {
			perror("listen");
			close(sockfd);
			sockfd = -1;
			continue;
		}
		break;
	}

	freeaddrinfo(res);

	return sockfd;
}

int socket_helper_tcp_connect(char *remote_host, char *remote_port)
{
	struct addrinfo hints, *res, *p;
	int status, sockfd = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; // ipv4 or ipv6
	hints.ai_socktype = SOCK_STREAM; // tcp connection

	if ((status = getaddrinfo(remote_host, remote_port, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		return -1;
	}

	for (p = res; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("socket");
			continue;
		}
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			perror("connect");
			close(sockfd);
			sockfd = -1;
			continue;
		}
		break;
        }

	freeaddrinfo(res);

	return sockfd;	
}

// blocking if 1 else non-blocking
int socket_helper_set_blocking(int sockfd, int blocking)
{
	int flags;
	if((flags = fcntl(sockfd, F_GETFL, 0)) == -1) {
		perror("fcntl");
		return -1;
	}
        flags = (blocking == 1) ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
	if (fcntl(sockfd, F_SETFL, flags) == -1) {
		perror("fcntl");
		return -1;
	}
	return 0;
}

int socket_helper_sendall(int sockfd, void *buffer, int len)
{
	int b;
	char *ptr = (char *)buffer;
	while (len > 0) {
		if ((b = send(sockfd, buffer, len, 0)) == -1) {
			perror("send");
			return -1;
		}
		ptr += b;
		len -= b;
	}
	return 0;
}

int socket_helper_recvall(int sockfd, void **buffer, int *len)
{
	char *lbuffer = malloc(sizeof(char) * SOCKET_HELPER_BUFFER_SIZE_INIT);
	uint size = SOCKET_HELPER_BUFFER_SIZE_INIT;
	uint left = size;
	char *ptr = lbuffer;
	int b;
	while (1) {
		if ((b = recv(sockfd, ptr, left, 0)) == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				break;
			}
			free(buffer);
			perror("recv");
			*buffer = NULL;
			*len = 0;
			return -1;
		}
		printf("%d %d %d\n", b, left, size);
		ptr += b;
		left -= b;
		if (left > 0) {
			break;
		}
		size += SOCKET_HELPER_BUFFER_SIZE_INCR;
		left += SOCKET_HELPER_BUFFER_SIZE_INCR;
		lbuffer = realloc(lbuffer, size);
		ptr = lbuffer + (size - left);
	}
	*buffer = lbuffer;
	*len = (size - left);
	return 0;
}

int socket_helper_accept(int sockfd, int **sockets)
{
	
}
