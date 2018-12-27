#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#define BACKLOG 10

void sigchld_handler(int s)
{
	int saved_errno = errno;
	while (waitpid(-1, NULL, WNOHANG) > 0)
		;
	errno = saved_errno;
}

int main(int argc, char const *argv[])
{
	int sock_fd, conn_fd;
	struct addrinfo hints, *servinfo, *i;
	struct sockaddr_storage conn_addr;
	socklen_t sin_size;
	struct sigaction sa;
	int yes = 1;
	char s[INET6_ADDRSTRLEN];
	int status;

	if (argc != 3)
	{
		fprintf(stderr, "Insufficient arguments supplied!\n");
		return 1;
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	status = getaddrinfo(NULL, argv[2], &hints, &servinfo);
	if (status != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		return 1;
	}

	for (i = servinfo; i != NULL; i = i->ai_next)
	{
		sock_fd = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
		if (sock_fd == -1)
		{
			perror("server: socket");
			continue;
		}

		if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		{
			perror("setsockopt");
			return 1;
		}

		if (bind(sock_fd, i->ai_addr, i->ai_addrlen) == -1)
		{
			close(sock_fd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo);

	if (i == NULL)
	{
		fprintf(stderr, "server: failed to bind\n");
		return 1;
	}

	if (listen(sock_fd, BACKLOG) == -1)
	{
		perror("listen");
		return 1;
	}

	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1)
	{
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while (1)
	{
		sin_size = sizeof conn_addr;
		conn_fd = accept(sock_fd, (struct sockaddr *)&conn_addr, &sin_size);
		if (conn_fd == -1)
		{
			perror("accept");
			continue;
		}

		printf("server: got new connection\n");

		if (fork() == 0)
		{
			close(sock_fd);
			if (send(conn_fd, "Ping", 4, 0) == -1)
				perror("send");
			close(conn_fd);
			exit(0);
		}

		close(conn_fd);
	}

	return 0;
}
