#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BACKLOG 10
#define BUFSIZE 1024

#define HEADER200 "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
#define HEADER404 "HTTP/1.1 404 Not Found\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"

struct request
{
	char type[10];
	char path[1024];
};

struct request recv_request(char buf[])
{
	struct request req;
	char delim[4] = " \t\n";
	char *token;
	token = strtok(buf, delim);
	strcpy(req.type, token);
	token = strtok(NULL, delim);
	strcpy(req.path, token);
	return req;
}

void sigchld_handler(int s)
{
	int saved_errno = errno;
	while (waitpid(-1, NULL, WNOHANG) > 0)
		;
	errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
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
	char *buffer = malloc(BUFSIZE);
	char header[BUFSIZE], content[BUFSIZE];
	int bytes;
	struct request req;
	char port[5], location[1024];
	int file;

	if (argc != 3)
	{
		fprintf(stderr, "Insufficient arguments supplied!\n");
		return 1;
	}

	stpcpy(location, argv[1]);
	strcpy(port, argv[2]);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	status = getaddrinfo(NULL, port, &hints, &servinfo);
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
			perror("cerver: socket");
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
			perror("cerver: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo);

	if (i == NULL)
	{
		fprintf(stderr, "cerver: failed to bind\n");
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

	printf("cerver: waiting for connections...\n");

	while (1)
	{
		sin_size = sizeof conn_addr;
		conn_fd = accept(sock_fd, (struct sockaddr *)&conn_addr, &sin_size);
		if (conn_fd == -1)
		{
			perror("accept");
			continue;
		}

		inet_ntop(conn_addr.ss_family, get_in_addr((struct sockaddr *)&conn_addr), s, sizeof s);
		printf("cerver: got connection from %s\n", s);

		if (fork() == 0)
		{
			close(sock_fd);

			memset(buffer, 0, BUFSIZE);
			if (recv(conn_fd, buffer, BUFSIZE, 0) == -1)
				perror("recv");

			req = recv_request(buffer);

			strcat(location, req.path);

			printf("::%s:: %s\n", req.type, location);

			if (location[strlen(location) - 1] == '/')
			{
				if ((file = open(strcat(location, "index.html"), O_RDONLY)) != -1)
					strcpy(header, HEADER200);
				else if ((file = open(strcat(location, "index.htm"), O_RDONLY)) != -1)
					strcpy(header, HEADER200);
			}
			else if ((file = open(location, O_RDONLY)) != -1)
				strcpy(header, HEADER200);
			else
				strcpy(header, HEADER404);

			if (send(conn_fd, header, strlen(header), 0) == -1)
				perror("send");
			else
			{
				if (strcmp(header, HEADER200) == 0)
				{
					while ((bytes = read(file, content, BUFSIZE)) > 0)
						write(conn_fd, content, bytes);
				}
				else
					write(conn_fd, "404 Page Not Found", 19);
			}

			close(conn_fd);
			exit(0);
		}

		close(conn_fd);
	}

	return 0;
}
