#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>

#include <unistd.h>

#include <sys/epoll.h>

#include "socket_helper.h"

#define SERVER_EPOLL_MAX_EVENTS 128
#define SERVER_PORT "3456"

#define SERVER_EPOLL_MAX_EVENTS_PER_RUN SERVER_EPOLL_MAX_EVENTS
#define SERVER_EPOLL_MAX_TIMEOUT 1000

sig_atomic_t volatile g_running = 0;

void sig_handler(int signo)
{
	switch (signo)
	{
	case SIGINT:
		g_running = 1;
		break;
	}
}

void handle_server_socket(int sockfd, int epfd)
{
	int *sockets;
        int nsocks = socket_helper_accept(sockfd, &sockets);
	
}

void handle_client_request(int sockfd)
{
	
}

int main(int argc, char **argv)
{
	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		perror("signal");
		return 1;
	}
	
	int sockfd = socket_helper_tcp_listen(SERVER_PORT);

	if (sockfd == -1) {
		fprintf(stderr, "socket_helper_tcp_listen error: Could Not Listen\n");
		return 1;
	}

	int epfd = epoll_create1(0);
	if (epfd < 0) {
		perror("epoll_create1");
		return 1;
	}

	struct epoll_event ev, *events;
	ev.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
	ev.data.fd = sockfd;
	int res = epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

	events = malloc(sizeof(struct epoll_event) * SERVER_EPOLL_MAX_EVENTS);

	int nfds;
	int ifds;
	int fd;

	while (!g_running) {
		nfds = epoll_wait(epfd, events, SERVER_EPOLL_MAX_EVENTS_PER_RUN, SERVER_EPOLL_MAX_TIMEOUT);

		for (ifds = 0; ifds < nfds; ifds++) {
			fd = events[ifds].data.fd;
			if (fd == sockfd) {
				
			} else {
				
			}
		}
	}


	free(events);
	
	close(epfd);

	close(sockfd);

	return 0;
}
