#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define BUF_SIZE 500

int main(int argc, char *argv[])
{
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s, j;
	size_t len;
	ssize_t nread;
	char buf[BUF_SIZE];
	int hostindex;
	int af;

	if (!(argc >= 3 || (argc >= 4 &&
						(strcmp(argv[1], "-4") == 0 || strcmp(argv[1], "-6") == 0))))
	{
		fprintf(stderr, "Usage: %s [ -4 | -6 ] host port msg...\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Use only IPv4 (AF_INET) if -4 is specified;
	 * Use only IPv6 (AF_INET6) if -6 is specified;
	 * Try both if neither is specified. */
	af = AF_UNSPEC;
	if (strcmp(argv[1], "-4") == 0 ||
		strcmp(argv[1], "-6") == 0)
	{
		if (strcmp(argv[1], "-6") == 0)
		{
			af = AF_INET6;
		}
		else
		{ // (strcmp(argv[1], "-4") == 0) {
			af = AF_INET;
		}
		hostindex = 2;
	}
	else
	{
		hostindex = 1;
	}

	/* Obtain address(es) matching host/port */

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = af;			 /* Allow IPv4, IPv6, or both, depending on
							what was specified on the command line. */
	hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
	hints.ai_flags = 0;
	hints.ai_protocol = 0; /* Any protocol */

	/* SECTION A - pre-socket setup; getaddrinfo() */

	s = getaddrinfo(argv[hostindex], argv[hostindex + 1], &hints, &result);
	if (s != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	/* getaddrinfo() returns a list of address structures.
	   Try each address until we successfully connect(2).
	   If socket(2) (or connect(2)) fails, we (close the socket
	   and) try the next address. */

	/* SECTION B - pre-socket setup; getaddrinfo() */
	for (rp = result; rp != NULL; rp = rp->ai_next)
	{
		// the system calls
		sfd = socket(rp->ai_family, rp->ai_socktype,
					 rp->ai_protocol);
		if (sfd == -1)
			continue;

		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break; /* Success */

		close(sfd);
	}

	if (rp == NULL)
	{ /* No address succeeded */
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(result); /* No longer needed */

	/* SECTION C - interact with server; send, receive, print messages */

	/* Send remaining command-line arguments as separate
	   datagrams, and read responses from server */

	char buffer[4096];

	// makes note of how many bytes were received from stdin and stored in the buffer
	int numBytesToWrite = fread(buffer, sizeof(char), 4096, stdin);

	// reads (using fread()) input from stdin into a buffer (char []) until EOF is reached (max total bytes 4096);
	// fread keeps tracks of the bytes that read from stdin

	int bytesWritten = 0;
	int temp;
	while ((temp = write(sfd, &buffer[0] + bytesWritten, (numBytesToWrite - bytesWritten))) > 0)
	{
		bytesWritten += temp;
		if (bytesWritten >= numBytesToWrite)
		{
			break;
		}
	}

	/* -- START #22 CODE -- */
	char readBuffer[16384];
	int numBytesToRead = 0;

	int bytesRead = 0;
	temp = 0;
	while ((temp = read(sfd, &readBuffer[0] + bytesRead, 512)) != 0)
	{
		bytesRead += temp;
		if (bytesRead >= 16384)
		{
			break;
		}
	}

	// The data you have read is not guaranteed to be null-terminated, so after all the content has been read,
	// if you want to use it with printf(), you should add the null termination yourself (i.e., at the index
	// indicated by the total bytes read).
	readBuffer[bytesRead] = '\0'; // Null terminator.

	// - after all the data has been read from the socket, you write the contents of the buffer to stdout.
	// Just like when you were communicating with your own server, the HTTP server you are communicating with
	// will close the connection when it is done sending data.  When this happens, recv() returns 0, for EOF.
	write(1, readBuffer, bytesRead);
	/* -- END #22 CODE -- */

	for (j = hostindex + 2; j < argc; j++)
	{
		// getting the length of command string
		len = strlen(argv[j]) + 1;
		/* +1 for terminating null byte */

		if (len + 1 > BUF_SIZE)
		{
			fprintf(stderr,
					"Ignoring long message in argument %d\n", j);
			continue;
		}

		// sdf - the socket im sending data to
		if (write(sfd, argv[j], len) != len)
		{
			fprintf(stderr, "partial/failed write\n");
			exit(EXIT_FAILURE);
		}

		// nread = read(sfd, buf, BUF_SIZE);
		// if (nread == -1)
		// {
		// 	perror("read");
		// 	exit(EXIT_FAILURE);
		// }

		printf("Received %zd bytes: %s\n", nread, buf);
	}

	exit(EXIT_SUCCESS);
}
