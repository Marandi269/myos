/*
 * init.c - First user process (PID 1)
 *
 * The init process is responsible for:
 * 1. Setting up stdin/stdout/stderr
 * 2. Starting the shell
 * 3. Reaping zombie processes
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>

/* Shell path */
#define SHELL_PATH "/bin/sh"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    int status;
    pid_t pid;

    /* Print banner */
    printf("\n");
    printf("================================\n");
    printf("  MyOS init (PID %d)\n", getpid());
    printf("================================\n");
    printf("\n");

    /* Open stdin/stdout/stderr to /dev/console */
    /* These should already be set up, but just in case */
    int fd = open("/dev/console", O_RDWR);
    if (fd >= 0) {
        if (fd != 0) {
            dup2(fd, 0);  /* stdin */
        }
        dup2(0, 1);       /* stdout */
        dup2(0, 2);       /* stderr */
        if (fd > 2) {
            close(fd);
        }
    }

    printf("[init] Starting system...\n");

    /* Main loop - spawn shell and reap children */
    while (1) {
        printf("[init] Spawning shell: %s\n", SHELL_PATH);

        pid = fork();
        if (pid < 0) {
            printf("[init] fork() failed!\n");
            continue;
        }

        if (pid == 0) {
            /* Child - exec shell */
            char *shell_argv[] = { "sh", NULL };
            execve(SHELL_PATH, shell_argv, NULL);

            /* If exec fails, try to print error and exit */
            printf("[init] Failed to exec %s\n", SHELL_PATH);
            exit(127);
        }

        /* Parent - wait for shell to exit */
        printf("[init] Shell started (PID %d)\n", pid);

        while (1) {
            pid_t waited = waitpid(-1, &status, 0);
            if (waited < 0) {
                break;  /* No more children */
            }

            if (waited == pid) {
                /* Shell exited */
                if (WIFEXITED(status)) {
                    printf("[init] Shell exited with status %d\n",
                           WEXITSTATUS(status));
                } else {
                    printf("[init] Shell terminated\n");
                }
                break;  /* Respawn shell */
            } else {
                /* Reaped a zombie */
                printf("[init] Reaped zombie (PID %d)\n", waited);
            }
        }

        printf("[init] Respawning shell...\n");
    }

    return 0;
}
