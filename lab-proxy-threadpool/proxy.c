#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define HOST_PREFIX 2
#define PORT_PREFIX 1
#define BUF_SIZE 1049000
#define NUM_THREADS 8
#define SBUFSIZE 5

//=======================Sbuf stuff ==========================//

typedef struct
{
    int *buf;
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
} sbuf_t;

void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = calloc(n, sizeof(int));
    sp->n = n;
    sp->front = sp->rear = 0;
    sem_init(&sp->mutex, 0, 1);
    sem_init(&sp->slots, 0, n);
    sem_init(&sp->items, 0, 0);
}

void sbuf_deinit(sbuf_t *sp)
{
    free(sp->buf);
}

void sbuf_insert(sbuf_t *sp, int item)
{
    printf("before wait(slots)\n");
    fflush(stdout);
    sem_wait(&sp->slots);
    printf("after wait(slots)\n");
    fflush(stdout);
    sem_wait(&sp->mutex);
    sp->buf[(++sp->rear) % (sp->n)] = item;
    sem_post(&sp->mutex);
    printf("before post(items)\n");
    fflush(stdout);
    sem_post(&sp->items);
    printf("after post(items)\n");
    fflush(stdout);
}

int sbuf_remove(sbuf_t *sp)
{
    int item;
    printf("before wait(items)\n");
    fflush(stdout);
    sem_wait(&sp->items);
    printf("after wait(items)\n");
    fflush(stdout);
    sem_wait(&sp->mutex);
    item = sp->buf[(++sp->front) % (sp->n)];
    sem_post(&sp->mutex);
    printf("before post(slots)\n");
    fflush(stdout);
    sem_post(&sp->slots);
    printf("after post(slots)\n");
    fflush(stdout);
    return item;
}

//============================================================//

sbuf_t sbuf;
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";

int all_headers_received(char *);
int parse_req(char *, char *, char *, char *, char *, char *);
int open_sfd();
void test_parser();
void print_bytes(unsigned char *, int);
void handle_client(int new_sfd);
void *detach_thread();

int main(int argc, char *argv[])
{
    // test_parser();
    printf("%s\n", user_agent_hdr);

    int sfd = open_sfd(argc, argv);
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len = sizeof(struct sockaddr_storage);
    pthread_t tid;

    sbuf_init(&sbuf, SBUFSIZE);
    //  every time a new client connects, pthread_create() is called
    for (unsigned int i = 0; i < NUM_THREADS; i++)
    {
        pthread_create(&tid, NULL, detach_thread, NULL);
    }

    while (1)
    {
        peer_addr_len = sizeof(struct sockaddr_storage);
        int clientFd = accept(sfd, (struct sockaddr *)&peer_addr, &peer_addr_len);
        sbuf_insert(&sbuf, clientFd);
    }
    return 0;
}

int all_headers_received(char *req)
{
    if (strstr(req, "\r\n\r\n") != NULL)
    {
        return 1;
    }
    return 0;
}

int parse_req(char *req, char *method,
              char *hostname, char *port, char *path, char *headers)
{

    if (all_headers_received(req) == 0)
    {
        return 0;
    }

    char *buf;
    int found = 0;
    unsigned int i = 0;
    while (i < strlen(req))
    {
        if (req[i] == ' ')
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
    strncpy(method, req, i);
    method[i] = '\0';

    buf = strstr(req, "//");
    i = HOST_PREFIX;
    int default_port = 0;
    found = 0;
    while (i < strlen(buf))
    {
        if (buf[i] == '/')
        {
            found = 1;
            default_port = 1;
            break;
        }
        else if (buf[i] == ':')
        {
            found = 1;
            default_port = 0;
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

    if (default_port == 1)
    {
        strcpy(port, "80"); // default port is set to use 80
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

    buf = strstr(req, "\r\n");
    strcpy(headers, &buf[2]);

    return 1;
}

void test_parser()
{
    int i;
    char method[16], hostname[64], port[8], path[64], headers[1024];

    char *reqs[] = {
        "GET http://www.example.com/index.html HTTP/1.0\r\n"
        "Host: www.example.com\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
        "Accept-Language: en-US,en;q=0.5\r\n\r\n",

        "GET http://www.example.com:8080/index.html?foo=1&bar=2 HTTP/1.0\r\n"
        "Host: www.example.com:8080\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
        "Accept-Language: en-US,en;q=0.5\r\n\r\n",

        "GET http://www.example.com:8080/index.html HTTP/1.0\r\n",

        NULL};

    for (i = 0; reqs[i] != NULL; i++)
    {
        printf("Testing %s\n", reqs[i]);
        if (parse_req(reqs[i], method, hostname, port, path, headers))
        {
            printf("METHOD: %s\n", method);
            printf("HOSTNAME: %s\n", hostname);
            printf("PORT: %s\n", port);
            printf("HEADERS: %s\n", headers);
        }
        else
        {
            printf("REQUEST INCOMPLETE\n");
        }
    }
}

// This function create and configure a TCP socket for listening and accepting new client connections
int open_sfd(int argc, char *argv[])
{
    int sfd, s;
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_family = AF_INET;       /* Allow IPv4, IPv6, or both, depending on
                       what was specified on the command line. */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0; /* Any protocol */

    if (argc < 1)
    {
        fprintf(stderr, "Command line argument error");
        exit(EXIT_FAILURE);
    }

    s = getaddrinfo(NULL, argv[1], &hints, &result);
    if (s != 0)
    {
        fprintf(stderr, "getaddrinfo error\n");
        exit(EXIT_FAILURE);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        if ((sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1)
        {
            continue;
        }
        else
            break;
    }
    //  set an option on the socket to allow it bind to an address and port already in use
    int optval = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    if (bind(sfd, result->ai_addr, result->ai_addrlen) < 0)
    {
        fprintf(stderr, "Could not bind");
        exit(EXIT_FAILURE);
    }

    listen(sfd, 100);

    return sfd;
}

void *detach_thread()
{
    // run your threads in detached mode to avoid memory leaks
    // When a new thread is spawned you can put it in detached mode by calling within the thread routine
    pthread_detach(pthread_self());
    while (1)
    {
        int new_sfd = sbuf_remove(&sbuf);
        handle_client(new_sfd);
    }
}

void handle_client(int new_sfd)
{
    char buf[BUF_SIZE];
    int nread = 0;
    // Read from the socket into a buffer until the entire HTTP req has been received
    while (1)
    {
        int tmp = 0;
        tmp = recv(new_sfd, &buf[nread], BUF_SIZE, 0);
        nread += tmp;

        if (all_headers_received(buf) == 1)
        {
            break;
        }
    }

    char method[16], hostname[64], port[8], path[64], headers[1024], new_req[BUF_SIZE];
    if (parse_req(buf, method, hostname, port, path, headers))
    {
        // if no port pass, use default port 80
        if (strcmp(port, "80"))
        {
            sprintf(new_req, "%s %s HTTP/1.0\r\nHost: %s:%s\r\nUser-Agent: %s\r\nConnection: close\r\nProxy-Connection: close\r\n\r\n",
                    method, path, hostname, port, user_agent_hdr);
        }
        else
        {
            sprintf(new_req, "%s %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\nConnection: close\r\nProxy-Connection: close\r\n\r\n",
                    method, path, hostname, user_agent_hdr);
        }
        printf("%s", new_req);
    }
    else
    {
        printf("REQUEST ERROR\n");
        close(new_sfd);
        return;
    }

    int socket_sfd, s;
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
        exit(EXIT_FAILURE);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        socket_sfd = socket(rp->ai_family, rp->ai_socktype,
                            rp->ai_protocol);
        if (socket_sfd == -1)
            continue;

        if (connect(socket_sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break; /* Success */

        close(socket_sfd);
    }

    write(socket_sfd, new_req, BUF_SIZE);
    char new_buf[BUF_SIZE];
    nread = 0;
    while (1)
    {
        int tmp = 0;
        tmp = recv(socket_sfd, &new_buf[nread], BUF_SIZE, 0);
        nread += tmp;

        if (tmp == 0)
        {
            break;
        }
    }

    write(new_sfd, new_buf, nread + 1);
    close(new_sfd);
    close(socket_sfd);
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