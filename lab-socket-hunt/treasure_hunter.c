// Replace PUT_USERID_HERE with your actual BYU CS user id, which you can find
// by running `id -u` on a CS lab machine.
#define USERID 1823692117
// hexa 6cb35555
#define BUF_SIZE 500
#define BUFSIZE 8

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int verbose = 0;

void print_bytes(unsigned char *bytes, int byteslen);

int main(int argc, char *argv[])
{

    /*** START OF LEVEL 0 *****/
    // setting Buffer
    unsigned char buf[BUFSIZE];
    unsigned char treasure[1024];
    int index = 0;
    long unsigned int hex_id = htonl(USERID);
    bzero(buf, BUFSIZE);
    struct sockaddr_in ipv4addr;

    memcpy(&buf[2], &hex_id, 4);
    unsigned char level = atoi(argv[3]);
    unsigned short seed = atoi(argv[4]);
    memcpy(&buf[1], &level, 1);
    memcpy(&buf[6], &seed, 2);

    /** SET UP UDP**/
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s;
    ssize_t nread;
    int af;
    struct sockaddr *address;
    socklen_t address_len;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;

    af = AF_INET;
    /* Obtain address(es) matching host/port */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = af;           /* Allow IPv4, IPv6, or both, depending on
                           what was specified on the command line. */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0; /* Any protocol */
    /* SECTION A - pre-socket setup; getaddrinfo() */

    s = getaddrinfo(argv[1], argv[2], &hints, &result);
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
        address = rp->ai_addr;
        address_len = rp->ai_addrlen;
        break;
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

    // the initial read and write for response

    if (sendto(sfd, buf, 8, 0, address, address_len) != 8)
    {
        fprintf(stderr, "partial/failed write\n");
        exit(EXIT_FAILURE);
    }
    unsigned char res_buf[64];
    // message receive
    nread = read(sfd, res_buf, 64);
    if (nread == 0)
    {
        perror("read");
        exit(EXIT_FAILURE);
    }
    unsigned int intial_var = 0;
    // parse chunk length
    memcpy(&intial_var, res_buf, 1);

    do
    {
        // sdf - the socket im sending data to
        unsigned int nonce;
        memcpy(&intial_var, res_buf, 1);
        // print_bytes(res_buf, 64);
        if (intial_var == 0 || intial_var > 127)
            break;
        /** extract nonce from treasure**/
        // nonce
        memcpy(&nonce, res_buf + intial_var + 4, 4);
        // flip the bytes
        nonce = ntohl(nonce);
        nonce = nonce + 1;
        // flip it back
        nonce = htonl(nonce);
        unsigned char chunk[intial_var];
        memcpy(chunk, res_buf + 1, intial_var);

        //// start level 1 - opt code here
        unsigned int opt_code;
        // get op params
        unsigned short params;
        memcpy(&opt_code, res_buf + intial_var + 1, 1);
        // printf("%d whats my opt code \n", opt_code);
        if (opt_code == 1)
        {
            memcpy(&params, res_buf + intial_var + 2, 2);
            // the convert to string
            char convert[6];
            sprintf(convert, "%d", ntohs(params));
            /* pre-socket setup; getaddrinfo() */
            s = getaddrinfo(argv[1], convert, &hints, &result);
            if (result)
            {
                address = result->ai_addr;
                address_len = result->ai_addrlen;
            }
        }
        //// start level 2 - opt code here
        else if (opt_code == 2)
        {
            memcpy(&params, res_buf + intial_var + 2, 2);
            // the convert to string
            char convert[6];
            sprintf(convert, "%d", ntohs(params));

            // close the old port
            close(sfd);
            if ((sfd = socket(result->ai_family, result->ai_socktype, 0)) < 0)
            {
                perror("Error creating socket");
                exit(EXIT_FAILURE);
            }
            ipv4addr.sin_family = AF_INET; // use AF_INET (IPv4)
            ipv4addr.sin_port = params;    // specific port
            ipv4addr.sin_addr.s_addr = 0;  // any/all local addresses
            // Then bind new socket to the old one
            if (bind(sfd, (struct sockaddr *)&ipv4addr, sizeof(struct sockaddr_in)) < 0)
            {
                perror("bind()");
            }
        }
        else if (opt_code == 3)
        {
            memcpy(&params, res_buf + intial_var + 2, 2);
            params = ntohs(params);
            // printf("%d print params\n", params);
            // start level 3 - opt code here to call recv n times
            nonce = 0;
            for (int i = 0; i < params; i++)
            {
                peer_addr_len = sizeof(struct sockaddr_storage);
                nread = recvfrom(sfd, buf, 8, 0,
                                 (struct sockaddr *)&peer_addr, &peer_addr_len);
                // printf("%d whats my receive from\n", nread);

                char host[NI_MAXHOST], service[NI_MAXSERV];
                s = getnameinfo((struct sockaddr *)&peer_addr,
                                peer_addr_len, host, NI_MAXHOST,
                                service, NI_MAXSERV, NI_NUMERICSERV | NI_NUMERICHOST);

                int ser_port = atoi(service);
                // printf("%d port\n", ser_port);
                // add all the port to get nonce
                nonce += ser_port;
            }
            nonce = nonce + 1;
            // flip it back
            nonce = htonl(nonce);
        }
        // append to buffer
        memset(buf, 0, 8);
        memcpy(buf, &nonce, 4);
        memcpy(treasure + index, &chunk, intial_var);
        index += intial_var;

        //// start the read and write again
        if (sendto(sfd, buf, 4, 0, address, address_len) != 4)
        {
            perror("sendto");
            printf("%d address_len value after opt \n", address_len);
            fprintf(stderr, "partial/failed write\n");
            exit(EXIT_FAILURE);
        }
        memset(res_buf, 0, 64);
        // message receive
        nread = read(sfd, res_buf, 64);
        if (nread == 0)
        {
            perror("read");
            exit(EXIT_FAILURE);
        }
    } while (intial_var > 0 && intial_var <= 127);
    treasure[index] = 0;
    printf("%s\n", treasure);
    /*** END OF LEVEL 0 *****/
    exit(EXIT_SUCCESS);
}

void print_bytes(unsigned char *bytes, int byteslen)
{
    int i, j, byteslen_adjusted;

    if (byteslen % 8)
    {
        byteslen_adjusted = ((byteslen / 8) + 1) * 8;
    }
    else
    {
        byteslen_adjusted = byteslen;
    }
    for (i = 0; i < byteslen_adjusted + 1; i++)
    {
        if (!(i % 8))
        {
            if (i > 0)
            {
                for (j = i - 8; j < i; j++)
                {
                    if (j >= byteslen_adjusted)
                    {
                        printf("  ");
                    }
                    else if (j >= byteslen)
                    {
                        printf("  ");
                    }
                    else if (bytes[j] >= '!' && bytes[j] <= '~')
                    {
                        printf(" %c", bytes[j]);
                    }
                    else
                    {
                        printf(" .");
                    }
                }
            }
            if (i < byteslen_adjusted)
            {
                printf("\n%02X: ", i);
            }
        }
        else if (!(i % 4))
        {
            printf(" ");
        }
        if (i >= byteslen_adjusted)
        {
            continue;
        }
        else if (i >= byteslen)
        {
            printf("   ");
        }
        else
        {
            printf("%02X ", bytes[i]);
        }
    }
    printf("\n");
}
