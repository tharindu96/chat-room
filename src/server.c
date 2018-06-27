#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include<pthread.h>
#include <errno.h>

#include <sys/epoll.h>

#include "user.h"


#define LISTENER_PRINTF(f, ...) printf("LISTENER: " f, ##__VA_ARGS__)
#define LISTENER_FPRINTF(s, f, ...) fprintf(s, "LISTENER: " f, ##__VA_ARGS__)
#define LISTENER_PERROR(s) perror("LISTENER: " s)

#define RESPONDER_PRINTF(f, ...) printf("RESPONDER: " f, ##__VA_ARGS__)
#define RESPONDER_FPRINTF(s, f, ...) fprintf(s, "RESPONDER: " f, ##__VA_ARGS__)
#define RESPONDER_PERROR(s) perror("RESPONDER: " s)

#define LISTENER_PORT "3456"
#define LISTENER_BACKLOG 10

#define RESPONDER_EVENTS_SIZE 512

void sig_handler(int signo);
// set blocking TRUE(1) or FALSE(0)
int set_blocking(int sockfd, int blocking);

// get a socket to listen on, it is non-blocking
int listener_get_socket();
// listener_thread
void *fn_listener(void *ptr);

void *fn_responder(void *ptr);

sig_atomic_t volatile g_running = 1;

int main(int argc, char **argv)
{

	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		perror("signal");
		return 1;
	}

	int iret;

	// init mutex locks
	iret = pthread_mutex_init(&user_lock_hashtable, NULL);
	if (iret != 0) {
		errno = iret;
		perror("pthread_mutex_init");
		g_running = 0;
		return 1;
	}

	iret = pthread_mutex_init(&user_lock_rename, NULL);
	if (iret != 0) {
		errno = iret;
		perror("pthread_mutex_init");
		g_running = 0;
		return 1;
	}

	pthread_t thread_listener, thread_responder;

	iret = pthread_create(&thread_listener, NULL, fn_listener, NULL);
	if (iret != 0) {
		errno = iret;
		perror("pthread_create");
		g_running = 0;
		return 1;
	}

	iret = pthread_create(&thread_responder, NULL, fn_responder, NULL);
	if (iret != 0) {
		errno = iret;
		perror("pthread_create");
		g_running = 0;
		return 1;
	}

	int listener_status;
	int responder_status;
	pthread_join(thread_listener, (void*)&listener_status);
	pthread_join(thread_responder, (void*)&responder_status);

	if (listener_status != 0 || responder_status != 0) {
		fprintf(stderr, "Error occured in one of the threads!\n");
		return 1;
	}

	pthread_mutex_destroy(&user_lock_hashtable);
	pthread_mutex_destroy(&user_lock_rename);

	return 0;
}

void sig_handler(int signo)
{
	switch (signo)
	{
	case SIGINT:
		g_running = 0;
		break;
	}
}

int set_blocking(int sockfd, int blocking)
{
	int flags;
	if ((flags = fcntl(sockfd, F_GETFL, 0)) == -1) {
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

/*** LISTENER CODE START ***/

int listener_get_socket()
{
	struct addrinfo hints;
	struct addrinfo *p, *res;
	int status, yes = 1, sockfd = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; // ipv4 or ipv6
	hints.ai_socktype = SOCK_STREAM; // use tcp protocol
	hints.ai_flags = AI_PASSIVE; // fill my ip

	if ((status = getaddrinfo(NULL, LISTENER_PORT, &hints, &res)) != 0) {
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
		if (listen(sockfd, LISTENER_BACKLOG) == -1) {
			perror("listen");
			close(sockfd);
			sockfd = -1;
			continue;
		}
		break;
	}

	freeaddrinfo(res);

	if (sockfd != -1 && set_blocking(sockfd, 0) != 0) {
		close(sockfd);
		sockfd = -1;
	}

	return sockfd;
}

void *fn_listener(void *ptr)
{
	LISTENER_PRINTF("starting...\n");

	int efd, listener_socket;
	struct epoll_event event;

	listener_socket = listener_get_socket();
	if (listener_socket == -1) {
		LISTENER_FPRINTF(stderr, "listener_get_socket error!\n");
		g_running = 0;
		return (void*) 1;
	}

	efd = epoll_create1(0);
	if (efd == -1) {
		LISTENER_PERROR("epoll_create1");
		g_running = 0;
		close(listener_socket);
		return (void*) 1;
	}

	event.data.fd = listener_socket;
	event.events = EPOLLIN | EPOLLET;
	if ((epoll_ctl(efd, EPOLL_CTL_ADD, listener_socket, &event)) == -1) {
		LISTENER_PERROR("epoll_ctl");
		g_running = 0;
		close(listener_socket);
		close(efd);
		return (void*) 1;
	}

	int c;

	LISTENER_PRINTF("listening...\n");

	while (g_running) {
		epoll_wait(efd, &event, 1, 100);

		while ((c = accept(listener_socket, NULL, NULL)) != -1) {
			LISTENER_PRINTF("accepted connection!\n");
			user_create(c);
		}

		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			continue;
		} else {
			LISTENER_PERROR("accept");
			g_running = 0;
			break;
		}
	}

	LISTENER_PRINTF("stopping...\n");

	close(efd);
	close(listener_socket);

	return 0;
}

/*** LISTENER CODE END ***/

/*** RESPONDER CODE START ***/

void *fn_responder(void *ptr)
{
	RESPONDER_PRINTF("starting...\n");

	int efd, listener_socket;
	struct epoll_event event;
	struct epoll_event *events = malloc(sizeof(struct epoll_event) * RESPONDER_EVENTS_SIZE);
	user *u;
	unsigned long c;
	int e, sockfd;

	efd = epoll_create1(0);
	if (efd == -1) {
		RESPONDER_PERROR("epoll_create1");
		g_running = 0;
		free(events);
		return (void*) 1;
	}

restart_responder_entry:

	if (g_running == 0) {
		goto stop_responder_entry;
	}

	u = user_get_next(NULL);

	if (u == NULL) {
		usleep(10000);
		goto restart_responder_entry;
	}

	c = user_get_count();

	for (unsigned long i = 0; i < c; i++) {
		if (u->added == 0) {
			u->added = 1;
			event.data.fd = u->sockfd;
			event.events = EPOLLIN | EPOLLET;
			if ((epoll_ctl(efd, EPOLL_CTL_ADD, u->sockfd, &event)) == -1) {
				RESPONDER_PERROR("epoll_ctl");
				g_running = 0;
				close(efd);
				free(events);
				return (void*) 1;
			}
		}
		u = user_get_next(u);
	}

	if ((e = epoll_wait(efd, events, RESPONDER_EVENTS_SIZE, 100)) == -1) {
		RESPONDER_PERROR("epoll_ctl");
		g_running = 0;
		close(efd);
		free(events);
		return (void*) 1;
	}

	for (int i = 0; i < e; i++) {

		RESPONDER_PRINTF("IN: %d OUT: %d RDHUP: %d PRI: %d ERR: %d HUP: %d ET: %d ONESHOT: %d WAKEUP: %d EXCLUSIVE: %d\n",
		events[i].events & EPOLLIN,
		events[i].events & EPOLLOUT,
		events[i].events & EPOLLRDHUP,
		events[i].events & EPOLLPRI,
		events[i].events & EPOLLERR,
		events[i].events & EPOLLHUP,
		events[i].events & EPOLLET,
		events[i].events & EPOLLONESHOT,
		events[i].events & EPOLLWAKEUP,
		events[i].events & EPOLLEXCLUSIVE);

		sockfd = events[i].data.fd;
		u = user_get_by_sockfd(sockfd);
		if (u == NULL) {
			RESPONDER_FPRINTF(stderr, "user_get_by_sockfd: not found!\n");
			g_running = 0;
			close(efd);
			free(events);
			return (void*) 1;
		}

		if (events[i].events & EPOLLERR ||
			events[i].events & EPOLLHUP ||
			!(events[i].events & EPOLLIN)) {
			RESPONDER_PRINTF("ERROR OCCURED\n");
			if ((epoll_ctl(efd, EPOLL_CTL_DEL, sockfd, NULL)) == -1) {
				RESPONDER_PERROR("epoll_ctl");
				g_running = 0;
				close(efd);
				free(events);
				return (void*) 1;
			}
			user_destroy(u);
			close(sockfd);
			continue;
		}

		/**
		TODO READ IN USER DATA
		**/

		char buffer[512];
		read(sockfd, buffer, 512);

		RESPONDER_PRINTF("%s\n", buffer);

	}

	goto restart_responder_entry;

stop_responder_entry:

	RESPONDER_PRINTF("stopping...\n");

	close(efd);
	free(events);

	return 0;
}

/*** RESPONDER CODE END ***/
