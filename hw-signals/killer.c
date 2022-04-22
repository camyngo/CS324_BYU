#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/wait.h>

void sigint_handler(int signum)
{
    // send SIGKILL to all processes in group, so this process and children
    // will terminate.  Use SIGKILL because SIGTERM and SIGINT (among
    // others) are overridden in the child.
    kill(-getpgid(0), SIGKILL);
}

int main(int argc, char *argv[])
{
    char *scenario = argv[1];
    int pid = atoi(argv[2]);

    struct sigaction sigact;

    // Explicitly set flags
    sigact.sa_flags = SA_RESTART;
    sigact.sa_handler = sigint_handler;
    // Override SIGINT, so that interrupting this process sends SIGKILL to
    // this one and, more importantly, to the child.
    sigaction(SIGINT, &sigact, NULL);

    switch (scenario[0])
    {
        /**
         * 1-2-25
         */
        case '0':
            kill(pid, SIGINT);
            break;
            /**
             * NO outputs
             */
        case '1':
            kill(pid, 12);
            sleep(1);
            kill(pid, 15);
            break;
            /**
             * 1-2
             */
        case '2':
            kill(pid, SIGINT);
            sleep(4);
            kill(pid, 12);
            sleep(1);
            kill(pid, 15);
            break;

            /**
             * 1-2-1-2
             */
        case '3':
            kill(pid, SIGINT);
            sleep(5);
            kill(pid, SIGINT);
            sleep(4);
            kill(pid, 12);
            sleep(1);
            kill(pid, 15);
            break;
            /**
             * 1-1-2-2
             */
        case '4':
            kill(pid, SIGINT);
            sleep(2);
            // cant use the same signal
            // need to replace with a new one if we wannt do 11-22
            kill(pid, SIGHUP);
            sleep(4);
            kill(pid, 12);
            sleep(1);
            kill(pid, 15);
            break;

            /**
             * 1
             */
        case '5':
            kill(pid, SIGINT);
            kill(pid, 12);
            sleep(1);
            kill(pid, 15);
            break;

            /**
             * 1-2-7-10
             */
        case '6':
            kill(pid, SIGINT);
            sleep(4);
            // there is no handler printing 7 and 10
            // so we will use the builin count down
            kill(pid, 10);
            sleep(1);
            kill(pid, 16);
            kill(pid, 12);
            sleep(1);
            kill(pid, 15);
            break;

            /**
             * 1-2-7
             */
        case '7':
            kill(pid, SIGINT);
            sleep(4);
            kill(pid, 10);
            kill(pid, 12);
            sleep(1);
            kill(pid, 15);
            break;

            /**
             * 1-2-6
             */
        case '8':
            kill(pid, SIGINT);
            sleep(5);
            // mute
            kill(pid, 31);
            sleep(1);
            kill(pid, 10);
            sleep(1);

            kill(pid, 30);
            sleep(1);
            kill(pid, SIGTERM);
            kill(pid, 12);
            sleep(1);
            kill(pid, 15);
            break;

            /**
             * 8-9-1-2
             * Restriction: you cannot use SIGINT or SIGHUP on this scenario!
             */
        case '9':
            kill(pid, 31);
            sleep(1);
            kill(pid, SIGQUIT);
            sleep(5);
            kill(pid, 31);
            sleep(4);
            kill(pid, 12);
            sleep(1);
            kill(pid, 15);
            break;
    }
    waitpid(pid, NULL, 0);
}
