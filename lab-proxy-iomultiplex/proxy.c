#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_EVENTS 64
#define MAX_LINES 2048
#define HOST_PREFIX 2
#define PORT_PREFIX 1

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";

struct request_info
{
	int client_fd;
	int server_fd;
	// 0=READ_REQUEST, 1=SEND_REQUEST, 2=READ_RESPONSE, 3=SEND_RESPONSE, 4=FINISHED
	int current_state;
	char read_buffer[MAX_CACHE_SIZE];
	char write_buffer[MAX_CACHE_SIZE];
	int total_write_amount;
	int written_bytes_server;
	int read_bytes_server;
	int read_bytes_client;
	int written_bytes_client;
};

struct epoll_event event;
struct epoll_event *events;

struct request_info *new_request;
struct request_info *active_request;

int sig_flag = 0;

int all_headers_received(char *);
int parse_request(char *, char *, char *, char *, char *, char *);
void print_bytes(unsigned char *, int);
int open_sfd(int argc, char *argv[]);
void handle_new_clients(int efd, int server_fd);
void handle_client(struct request_info *request, int epoll_reading_fd, int epoll_write_fd);

void READ_REQUEST(struct request_info *request, int epoll_write_fd);
void SEND_REQUEST(struct request_info *request, int epoll_reading_fd);
void READ_RESPONSE(struct request_info *request, int epoll_write_fd);
void SEND_RESPONSE(struct request_info *request);

void sig_handler(int sig)
{
	sig_flag = 1;
}

int main(int argc, char *argv[])
{
	struct sigaction sig_act;
	sig_act.sa_flags = SA_RESTART;

	sig_act.sa_handler = sig_handler;
	sigaction(SIGINT, &sig_act, NULL);

	int epoll_reading_fd;
	if ((epoll_reading_fd = epoll_create1(0)) < 0)
	{
		fprintf(stderr, "create epoll error\n");
		exit(1);
	}

	int epoll_write_fd;
	if ((epoll_write_fd = epoll_create1(0)) < 0)
	{
		fprintf(stderr, "create epoll error\n");
		exit(1);
	}

	// OPEN LISTENING POCKET
	int listening_fd = open_sfd(argc, argv);
	new_request = malloc(sizeof(struct request_info));
	new_request->client_fd = listening_fd;

	event.data.ptr = new_request;
	event.events = EPOLLIN | EPOLLET;
	if (epoll_ctl(epoll_reading_fd, EPOLL_CTL_ADD, listening_fd, &event) < 0)
	{
		fprintf(stderr, "error adding event\n");
		exit(1);
	}

	events = calloc(MAX_EVENTS, sizeof(struct epoll_event));

	while (1)
	{
		int result = epoll_wait(epoll_reading_fd, events, MAX_EVENTS, 1000);

		if (result < 0)
		{
			if (errno == EBADF)
			{
				fprintf(stderr, "error: EBADF\n");
				break;
			}
			else if (errno == EFAULT)
			{
				fprintf(stderr, "error: EFAULT\n");
				break;
			}
			else if (errno == EINTR)
			{
				fprintf(stderr, "error: EINTR\n");
				break;
			}
			else if (errno == EINVAL)
			{
				fprintf(stderr, "error: EINVAL\n");
				break;
			}
		}
		else if (result == 0)
		{
			if (sig_flag)
			{
				break;
			}
		}

		for (int i = 0; i < result; ++i)
		{
			active_request = (struct request_info *)(events[i].data.ptr);

			if ((events[i].events & EPOLLERR) ||
				(events[i].events & EPOLLHUP) ||
				(events[i].events & EPOLLRDHUP))
			{
				fprintf(stderr, "epoll error%d\n", active_request->client_fd);
				close(active_request->client_fd);
				close(active_request->server_fd);
				free(active_request);
				continue;
			}

			if (active_request->client_fd == listening_fd)
			{
				handle_new_clients(epoll_reading_fd, listening_fd);
			}
			else
			{
				handle_client(active_request, epoll_reading_fd, epoll_write_fd);
			}
		}
	}
	free(events);
	close(epoll_reading_fd);
	close(epoll_write_fd);
	close(listening_fd);
	return 0;
}

int open_sfd(int argc, char *argv[])
{
	int server_fd, s;
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = 0;

	if (argc < 1)
	{
		fprintf(stderr, "Missing argument");
		exit(1);
	}

	s = getaddrinfo(NULL, argv[1], &hints, &result);
	if (s != 0)
	{
		fprintf(stderr, "error getaddrinfo");
		exit(1);
	}

	for (rp = result; rp != NULL; rp = rp->ai_next)
	{
		server_fd = socket(rp->ai_family, rp->ai_socktype,
						   rp->ai_protocol);
		if (server_fd == -1)
		{
			continue;
		}
		break;
	}

	// set an option on the socket to allow it bind to an address and port
	int optval = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

	// set listening file descriptor non-blocking
	if (fcntl(server_fd, F_SETFL, fcntl(server_fd, F_GETFL, 0) | O_NONBLOCK) < 0)
	{
		fprintf(stderr, "set up socket error\n");
		exit(1);
	}

	if (bind(server_fd, result->ai_addr, result->ai_addrlen) < 0)
	{
		fprintf(stderr, "bind error");
		exit(1);
	}

	// accepting new client
	listen(server_fd, 100);

	// set listening file descriptor non-blocking
	if (fcntl(server_fd, F_SETFL, fcntl(server_fd, F_GETFL, 0) | O_NONBLOCK) < 0)
	{
		fprintf(stderr, "set up socket error\n");
		exit(1);
	}

	return server_fd;
}

// Accept and prepare for communication with incoming clients.
void handle_new_clients(int efd, int listening_fd)
{
	while (1)
	{
		socklen_t clientlen = sizeof(struct sockaddr_storage);
		struct sockaddr_storage clientaddr;
		int nserver_fd = accept(listening_fd, (struct sockaddr *)&clientaddr, &clientlen);

		if (nserver_fd < 0)
		{
			// there are no more clients currently pending, break
			if (errno == EWOULDBLOCK ||
				errno == EAGAIN)
				break;
			else
			{
				fprintf(stderr, "error");
				exit(EXIT_FAILURE);
			}
		}

		if (fcntl(nserver_fd, F_SETFL, fcntl(nserver_fd, F_GETFL, 0) | O_NONBLOCK) < 0)
		{
			fprintf(stderr, "set up socket error\n");
			exit(1);
		}

		new_request = (struct request_info *)malloc(sizeof(struct request_info));
		new_request->client_fd = nserver_fd;
		fprintf(stderr, "new file descriptor : %d\n", new_request->client_fd);

		event.data.ptr = new_request;
		event.events = EPOLLIN | EPOLLET;
		if (epoll_ctl(efd, EPOLL_CTL_ADD, nserver_fd, &event) < 0)
		{
			fprintf(stderr, "error adding event\n");
			exit(1);
		}
	}
}

void handle_client(struct request_info *request, int epoll_reading_fd, int epoll_write_fd)
{
	if (request->current_state == 0)
	{
		READ_REQUEST(request, epoll_write_fd);
	}
	if (request->current_state == 1)
	{
		SEND_REQUEST(request, epoll_reading_fd);
	}
	if (request->current_state == 2)
	{
		READ_RESPONSE(request, epoll_write_fd);
	}
	if (request->current_state == 3)
	{
		SEND_RESPONSE(request);
	}
	if (request->current_state == 4)
	{
		fprintf(stderr, "\n in current_state 4\n");
	}
}

void READ_REQUEST(struct request_info *request, int epoll_write_fd)
{
	fprintf(stderr, "in client read request:  %d\n", request->client_fd);
	fflush(stderr);

	while (1)
	{
		int len = recv(request->client_fd, &request->read_buffer[request->read_bytes_client], MAX_LINES, 0);

		if (len < 0)
		{
			// no more data ready to be read
			if (errno == EWOULDBLOCK || errno == EAGAIN)
				return;
			else
			{
				// cancel client request, and deregister socket.
				fprintf(stderr, "in client recv : %d\n", request->client_fd);
				close(request->client_fd);
				free(request);
			}
			return;
		}

		request->read_bytes_client = request->read_bytes_client + len;
		if (all_headers_received(request->read_buffer) == 1)
		{
			break;
		}
	}

	char method[16], hostname[64], port[8], path[64], headers[1024];
	// parse the client request and create the request that will send to the server
	if (parse_request(request->read_buffer, method, hostname, port, path, headers))
	{
		if (strcmp(port, "80"))
		{
			sprintf(request->write_buffer, "%s %s HTTP/1.0\r\nHost: %s:%s\r\nUser-Agent: %s\r\nConnection: close\r\nProxy-Connection: close\r\n\r\n",
					method, path, hostname, port, user_agent_hdr);
		}
		else
		{
			sprintf(request->write_buffer, "%s %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\nConnection: close\r\nProxy-Connection: close\r\n\r\n",
					method, path, hostname, user_agent_hdr);
		}
		request->total_write_amount = strlen(request->write_buffer);
		printf("%s", request->write_buffer);
	}
	else
	{
		printf("request error\n");
		close(request->client_fd);
		free(request);
		return;
	}

	int nserver_fd, s;
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = 0;

	s = getaddrinfo(hostname, port, &hints, &result);
	if (s != 0)
	{
		fprintf(stderr, "getaddrinfo error\n");
		close(request->client_fd);
		free(request);
		return;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next)
	{
		nserver_fd = socket(rp->ai_family, rp->ai_socktype,
							rp->ai_protocol);
		if (nserver_fd == -1)
			continue;

		if (connect(nserver_fd, rp->ai_addr, rp->ai_addrlen) != -1)
			break; /* Success */

		close(nserver_fd);
	}

	if (fcntl(nserver_fd, F_SETFL, fcntl(nserver_fd, F_GETFL, 0) | O_NONBLOCK) < 0)
	{
		fprintf(stderr, "set up socket error\n");
		close(request->client_fd);
		free(request);
		return;
	}

	request->server_fd = nserver_fd;
	event.data.ptr = request;
	event.events = EPOLLOUT | EPOLLET;
	if (epoll_ctl(epoll_write_fd, EPOLL_CTL_ADD, nserver_fd, &event) < 0)
	{
		fprintf(stderr, "event error\n");
		close(request->client_fd);
		close(request->server_fd);
		free(request);
		return;
	}
	memset(request->read_buffer, '\0', request->read_bytes_client);
	request->current_state++;
}

void SEND_REQUEST(struct request_info *request, int epoll_reading_fd)
{
	fprintf(stderr, "in send request: %d\n", request->server_fd);
	while (1)
	{
		int len = write(request->server_fd, &request->write_buffer[request->written_bytes_server], request->total_write_amount);

		if (len < 0)
		{
			if (errno == EWOULDBLOCK || errno == EAGAIN)
				return;
			else
			{
				fprintf(stderr, "in server write: %d\n", request->server_fd);
				close(request->client_fd);
				close(request->server_fd);
				free(request);
			}
			return;
		}

		request->written_bytes_server += len;
		if (request->written_bytes_server >= request->total_write_amount)
		{
			break;
		}
	}

	event.data.ptr = request;
	event.events = EPOLLIN | EPOLLET;
	if (epoll_ctl(epoll_reading_fd, EPOLL_CTL_ADD, request->server_fd, &event) < 0)
	{
		fprintf(stderr, "error event\n");
		exit(1);
	}

	memset(request->write_buffer, '\0', request->written_bytes_server);
	request->current_state++;
}

void READ_RESPONSE(struct request_info *request, int epoll_write_fd)
{
	fprintf(stderr, "in client read request: %d\n", request->client_fd);
	while (1)
	{
		int len = recv(request->server_fd, &request->read_buffer[request->read_bytes_server], MAX_LINES, 0);

		if (len < 0)
		{
			if (errno == EWOULDBLOCK || errno == EAGAIN)
				return;
			else
			{
				fprintf(stderr, "in server recv : %d\n", request->server_fd);
				close(request->client_fd);
				free(request);
			}
			return;
		}

		if (len == 0)
		{

			break;
		}

		request->read_bytes_server += len;
	}

	event.data.ptr = request;
	event.events = EPOLLOUT | EPOLLET;
	if (epoll_ctl(epoll_write_fd, EPOLL_CTL_ADD, request->client_fd, &event) < 0)
	{
		fprintf(stderr, "event error\n");
		exit(1);
	}

	request->total_write_amount = request->read_bytes_server;
	request->current_state++;
}

void SEND_RESPONSE(struct request_info *request)
{
	fprintf(stderr, "in client send resquest: %d\n", request->client_fd);
	while (1)
	{
		int len = write(request->client_fd, &request->read_buffer[request->written_bytes_client], request->total_write_amount);

		if (len < 0)
		{
			if (errno == EWOULDBLOCK || errno == EAGAIN)
				return;
			else
			{
				fprintf(stderr, "in client write : %d\n", request->client_fd);
				close(request->client_fd);
				close(request->server_fd);
				free(request);
			}
			return;
		}

		request->written_bytes_client += len;
		if (request->written_bytes_client >= request->total_write_amount)
		{
			break;
		}
	}

	fprintf(stderr, "finished writing\n");
	fflush(stderr);

	close(request->client_fd);
	close(request->server_fd);
	request->current_state++;
	fprintf(stderr, "done with all request\n");
	fflush(stderr);
}

int all_headers_received(char *request)
{
	if (strstr(request, "\r\n\r\n") != NULL)
	{
		return 1;
	}
	return 0;
}

int parse_request(char *request, char *method,
				  char *hostname, char *port, char *path, char *headers)
{
	if (all_headers_received(request) == 0)
	{
		return 0;
	}

	char *buf;
	int found = 0;
	unsigned int i = 0;
	while (i < strlen(request))
	{
		if (request[i] == ' ')
		{
			found = 1;
			break;
		}
		++i;
	}
	if (found != 1)
	{
		return 0;
	}
	strncpy(method, request, i);
	method[i] = '\0';

	buf = strstr(request, "//");
	i = HOST_PREFIX;
	int defaultPort = 0;
	found = 0;
	while (i < strlen(buf))
	{
		if (buf[i] == '/')
		{
			found = 1;
			defaultPort = 1;
			break;
		}
		else if (buf[i] == ':')
		{
			found = 1;
			defaultPort = 0;
			break;
		}
		++i;
	}
	if (found != 1)
	{
		return 0;
	}
	strncpy(hostname, &buf[HOST_PREFIX], i - HOST_PREFIX);
	hostname[i - HOST_PREFIX] = '\0';

	if (defaultPort == 1)
	{
		strcpy(port, "80"); // default port
		buf = &buf[i];
	}
	else
	{
		found = 0;
		buf = strchr(buf, ':');
		i = PORT_PREFIX;
		while (i < strlen(buf))
		{
			if (buf[i] == '/')
			{
				found = 1;
				break;
			}
			++i;
		}
		if (found != 1)
		{
			return 0;
		}
		strncpy(port, &buf[1], i - PORT_PREFIX);
		port[i - PORT_PREFIX] = '\0';
		buf = &buf[i];
	}

	found = 0;
	i = 0;
	while (i < strlen(buf))
	{
		if (buf[i] == ' ')
		{
			found = 1;
			break;
		}
		++i;
	}
	strncpy(path, &buf[0], i);
	path[i] = '\0';

	buf = strstr(request, "\r\n");
	strcpy(headers, &buf[2]);

	return 1;
}
