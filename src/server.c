#include <stdio.h>
#include <signal.h>

void sig_handler(int signo);

sig_atomic_t volatile g_running = 0;

/**
 * TODO: Write the listener functions
 * 
 * breakdown of the listener
 * - getaddrinfo to get a suitable local interface
 * - 
 **/

int main(int argc, char **argv)
{

	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		perror("signal");
		return 1;
	}

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