#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include<pthread.h>
#include <errno.h>


#define LISTENER_PRINTF(f, ...) printf("LISTENER: " f, ##__VA_ARGS__)
#define LISTENER_FPRINTF(s, f, ...) fprintf(s, "LISTENER: " f, ##__VA_ARGS__)

#define RESPONDER_PRINTF(f, ...) printf("RESPONDER: " f, ##__VA_ARGS__)
#define RESPONDER_FPRINTF(s, f, ...) fprintf(s, "RESPONDER: " f, ##__VA_ARGS__)

#define LISTENER_PORT "3456"
#define LISTENER_BACKLOG 10

void sig_handler(int signo);
// set blocking TRUE(1) or FALSE(0)
int set_blocking(int sockfd, int blocking);

// get a socket to listen on, it is non-blocking
int listener_get_socket();
// listener_thread
void *fn_listener(void *ptr);

sig_atomic_t volatile g_running = 1;

int main(int argc, char **argv)
{

	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		perror("signal");
		return 1;
	}

	pthread_t thread_listener, thread_responder;
	int iret_listener, iret_responder;

	iret_listener = pthread_create(&thread_listener, NULL, fn_listener, NULL);
	if (iret_listener != 0) {
		errno = iret_listener;
		perror("pthread_create");
		return 1;
	}

	int listener_status;
	int responder_status;
	pthread_join(thread_listener, (void*)&listener_status);
	pthread_join(thread_responder, (void*)&responder_status);

	if (listener_status != 0 || responder_status != 0) {
		fprintf(stderr, "Error occured in on of the threads!\n");
		return 1;
	}

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

	int listener_socket = listener_get_socket();
	if (listener_socket == -1) {
		LISTENER_FPRINTF(stderr, "listener_get_socket error!\n");
		g_running = 0;
		return (void*)1;
	}

	

	LISTENER_PRINTF("stopping...\n");

	close(listener_socket);

	return 0;
}

/*** LISTENER CODE END ***/

/*** RESPONDER CODE START ***/

/*** RESPONDER CODE END ***/
