#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/epoll.h>

#define SERVER_EPOLL_MAX_EVENTS 128
#define SERVER_PORT "3456"

#define SERVER_EPOLL_MAX_EVENTS_PER_RUN SERVER_EPOLL_MAX_EVENTS
#define SERVER_EPOLL_MAX_TIMEOUT 1000

#define SERVER_BACKLOG 10
#define SERVER_BUFFER_SIZE_INIT 512
#define SERVER_BUFFER_SIZE_INCR 512

int server_tcp_listen(char *local_port);
int server_tcp_connect(char *remote_host, char *remote_port);
int server_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int server_set_blocking(int sockfd, int blocking);
int server_sendall(int sockfd, void *buffer, int len);
int server_recvall(int sockfd, void **buffer, int *len);

void sig_handler(int signo);

void handle_listener(int sockfd, int epfd, struct epoll_event *event);
void handle_request(int sockfd, int epfd, struct epoll_event *event);

sig_atomic_t volatile g_running = 0;

int main(int argc, char **argv)
{
	int nfds, ifds, fd, sockfd, epfd;
	struct epoll_event ev, *events;


	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		perror("signal");
		return 1;
	}

	sockfd = server_tcp_listen(SERVER_PORT);

	if (sockfd == -1) {
		fprintf(stderr, "server_tcp_listen error: Could Not Listen\n");
		return 1;
	}

	epfd = epoll_create1(0);
	if (epfd < 0) {
		perror("epoll_create1");
		return 1;
	}

	ev.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
	ev.data.fd = sockfd;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
		perror("epoll_ctl");
		return 1;
	}

	events = malloc(sizeof(struct epoll_event) * SERVER_EPOLL_MAX_EVENTS);

	while (!g_running) {
		nfds = epoll_wait(epfd, events, SERVER_EPOLL_MAX_EVENTS_PER_RUN, SERVER_EPOLL_MAX_TIMEOUT);

		for (ifds = 0; ifds < nfds; ifds++) {
			fd = events[ifds].data.fd;
			if (fd == sockfd) {
				handle_listener(fd, epfd, &events[ifds]);
			} else {
				handle_request(fd, epfd, &events[ifds]);
			}
		}
	}


	free(events);

	close(epfd);

	close(sockfd);

	return 0;
}

int server_tcp_listen(char *local_port)
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
		if (listen(sockfd, SERVER_BACKLOG) == -1) {
			perror("listen");
			close(sockfd);
			sockfd = -1;
			continue;
		}
		break;
	}

	freeaddrinfo(res);

	// set the socket to be non-blocking
	if (sockfd != -1 && server_set_blocking(sockfd, 0) != 0) {
		close(sockfd);
		sockfd = -1;
	}

	return sockfd;
}

int server_tcp_connect(char *remote_host, char *remote_port)
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

	// set the socket to be non-blocking
	if (sockfd != -1 && server_set_blocking(sockfd, 0) != 0) {
		close(sockfd);
		sockfd = -1;
	}

	return sockfd;
}

int server_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	int new_sock;
	if ((new_sock = accept(sockfd, addr, addrlen)) == -1) {
		perror("accept");
	}
	// set the socket to be non-blocking
	if (new_sock != -1 && server_set_blocking(new_sock, 0) != 0) {
		close(new_sock);
		new_sock = -1;
	}
	return new_sock;
}

// blocking if 1 else non-blocking
int server_set_blocking(int sockfd, int blocking)
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

int server_sendall(int sockfd, void *buffer, int len)
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

int server_recvall(int sockfd, void **buffer, int *len)
{
	*buffer = malloc(sizeof(char) * SERVER_BUFFER_SIZE_INIT);
	uint size = SERVER_BUFFER_SIZE_INIT;
	uint left = size;
	char *ptr = *buffer;
	int b;
	while (1) {
		if ((b = recv(sockfd, ptr, left, 0)) == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				break;
			}
			free(*buffer);
			perror("recv");
			*buffer = NULL;
			*len = 0;
			return -1;
		}
		ptr += b;
		left -= b;
		if (left > 0) {
			break;
		}
		size += SERVER_BUFFER_SIZE_INCR;
		left += SERVER_BUFFER_SIZE_INCR;
		*buffer = realloc(*buffer, size);
		ptr = *buffer + (size - left);
	}
	*len = (size - left);
	return 0;
}

void sig_handler(int signo)
{
	switch (signo)
	{
	case SIGINT:
		g_running = 1;
		break;
	}
}

void handle_listener(int sockfd, int epfd, struct epoll_event *event)
{
	int new_conn = 1;
	char *hello = "Hello Client!\n";
	int mlen = strlen(hello);
	struct epoll_event ev;

	struct sockaddr_storage addr;
	socklen_t addrlen;

	struct sockaddr_in *addr4 = NULL;
	struct sockaddr_in6 *addr6 = NULL;

	while (new_conn != -1) {
		new_conn = server_accept(sockfd, (struct sockaddr *)&addr, &addrlen);
		if (new_conn == -1) break;

		addr4 = NULL;
		addr6 = NULL;
		if (addr.ss_family == AF_INET) {
			addr4 = (struct sockaddr_in*)&addr;
			printf("%d\n", ntohs(addr4->sin_port));
		} else if (addr.ss_family == AF_INET6) {
			addr6 = (struct sockaddr_in6*)&addr;
			printf("%d\n", ntohs(addr6->sin6_port));
		}

		ev.events = EPOLLIN | EPOLLRDHUP | EPOLLPRI | EPOLLERR | EPOLLHUP;
		ev.data.fd = new_conn;
		if (epoll_ctl(epfd, EPOLL_CTL_ADD, new_conn, &ev) == -1) {
			perror("epoll_ctl");
			close(new_conn);
			continue;
		}
		server_sendall(new_conn, hello, mlen);
	}
}

void handle_request(int sockfd, int epfd, struct epoll_event *event)
{
	char *m;
	int len;

	if (event->events & EPOLLRDHUP) {
		if (epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL) == -1) {
			perror("epoll_ctl");
		}
		close(sockfd);
		return;
	}

	server_recvall(sockfd, (void**)&m, &len);

	printf("%d\n", len);

	if (strcmp(m, "exit\r\n") == 0) {
		if (epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL) == -1) {
			perror("epoll_ctl");
		}
		close(sockfd);
	}

	free(m);
}
