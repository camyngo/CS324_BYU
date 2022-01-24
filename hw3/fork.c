#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    int pid;
    int children_stat;
    int pipe_id[2];

    printf("Starting program; process has pid %d\n", getpid());
    FILE *fileName = fopen("fork-output.txt", "w+");
    fprintf(fileName, "%s", "BEFORE FORK\n");
    fflush(fileName);

    pipe(pipe_id);

    if ((pid = fork()) < 0)
    {
        fprintf(stderr, "Could not fork()");
        exit(1);
    }

    /* BEGIN SECTION A */
    fprintf(fileName, "%s", "SECTION A\n");
    fflush(fileName);

    printf("Section A;  pid %d\n", getpid());

    /* END SECTION A */
    if (pid == 0)
    {
        /* BEGIN SECTION B */
        printf("Section B; pid %d\n", getpid());

        close(pipe_id[0]);
        FILE *fileone = fdopen(pipe_id[1], "w");
        fputs("hello from Section B\n", fileone);

        fprintf(fileName, "%s", "SECTION B\n");
        fflush(fileName);
        printf("Section B done sleeping\n");

        /* exec.c code import */
        char *newenviron[] = {NULL};
        sleep(30);

        if (argc <= 1)
        {
            printf("No program to exec.  Exiting...\n");
            exit(0);
        }

        printf("Running exec of \"%s\"\n", argv[1]);

        int file_descriptor = fileno(fileName);
        dup2(file_descriptor, 1);

        execve(argv[1], &argv[1], newenviron);
        printf("End of program \"%s\".\n", argv[0]);
        /* end exec.c code */

        exit(0);

        /* END SECTION B */
    }
    else
    {
        /* BEGIN SECTION C */
        fprintf(fileName, "%s", "SECTION C\n");
        fflush(fileName);

        close(pipe_id[1]);
        FILE *fd;
        fd = fdopen(pipe_id[0], "r");
        char buf[100];
        fgets(buf, 100, fd);
        fputs(buf, stdout);

        //read(pipefd[0], buf, 100);
        // printf("Concatenated string %s\n",buf);
        wait(&children_stat);
        printf("Section C; pid %d\n", getpid());

        printf("Section C done sleeping\n");
        exit(0);
        /* END SECTION C */
    }
    /* BEGIN SECTION D */
    fprintf(fileName, "%s", "SECTION D\n");
    fflush(fileName);
    printf("Section D;pid %d\n", getpid());
    /* END SECTION D */
}
