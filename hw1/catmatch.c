#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
Part 1:
1. What are the numbers associated with the manual sections for executable programs, system calls, and library calls, respectively?
	- Numbers associated with executable programs are #1, system calls #2, library calls #3
2. Which section number(s) contain an entry for open?
	- Open(2)
3. What three #include lines should be included to use the open() system call?
	- #include <sys/types.h>
	- #include <sys/stat.h>
	- #include <sys/fcntl.h>
	
4. Which section number(s) contain an entry for socket?
	- Socket(2)
	- Socket(7)
5. Which socket option "Returns a value indicating whether or not this socket has been marked to accept connections with listen(2)"?
	- SO_ACCEPTCONN Returns a value indicating whether or not this socket has been marked to accept connections with listen(2).
6. How many sections contain keyword references to getaddrinfo?
	- getaddrinfo(3) : getaddrinfo(3) configuration file
	- getaddrinfo_a(3)
	- gai.conf(5)
7. According to the "DESCRIPTION" section of the man page for string, the functions described in that man page are used to perform operations on strings that are ________________. (fill in the blank)
	-  null-terminated strings
8. What is the return value of strcmp() if the value of its first argument (i.e., s1) is greater than the value of its second argument (i.e., s2)?
• a positive value if s1 is greater than s2.

Part 2: I completed the TMUX exercise from Part 2
*
*
*/

int main(int argc, char *argv[])
{
    char *CATMATCH_PATTERN[1000];
    int ipd = getpid();
    fprintf(stderr, "%d\n\n", ipd);
    char *result = getenv("CATMATCH_PATTERN");
    if (result == NULL)
    {
        putenv("CATMATCH_PATTERN=0");
        result = getenv("CATMATCH_PATTERN");
    }

    // argv take in the stuff from the command line
    if (argc < 2)
    {
        printf("Error open file \n");
    }
    else
    {
        FILE *fp = fopen(argv[1], "r");
        // Verify that a full line has been read ...
        char result2[256];
        while (fgets(result2, 256, fp))
        {
            if (strstr(result2, result) == NULL)
                printf("0 %s", result2);
            else
            {
                printf("1 %s", result2);
            }
        }
    }
}