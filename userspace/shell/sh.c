/*
 * sh.c - Simple shell
 *
 * A basic command-line interpreter supporting:
 * - Built-in commands: cd, pwd, exit, help, kill
 * - External command execution
 * - Pipe support (cmd1 | cmd2)
 * - Simple command parsing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 256
#define MAX_ARGS 32
#define MAX_PIPES 8
#define PROMPT "$ "

/* Forward declarations for syscalls not in headers */
extern int pipe(int pipefd[2]);
extern int kill(int pid, int sig);

/* Built-in command handlers */
static int builtin_cd(char **args);
static int builtin_pwd(char **args);
static int builtin_exit(char **args);
static int builtin_help(char **args);
static int builtin_kill(char **args);

/* Built-in command table */
static struct {
    const char *name;
    int (*handler)(char **args);
    const char *help;
} builtins[] = {
    { "cd",    builtin_cd,   "Change directory" },
    { "pwd",   builtin_pwd,  "Print working directory" },
    { "exit",  builtin_exit, "Exit shell" },
    { "help",  builtin_help, "Show this help" },
    { "kill",  builtin_kill, "Send signal to process" },
    { NULL,    NULL,         NULL }
};

/* cd command */
static int builtin_cd(char **args) {
    const char *path = args[1] ? args[1] : "/";
    if (chdir(path) != 0) {
        printf("cd: %s: No such file or directory\n", path);
        return 1;
    }
    return 0;
}

/* pwd command */
static int builtin_pwd(char **args) {
    (void)args;
    char cwd[256];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("%s\n", cwd);
        return 0;
    }
    printf("pwd: error\n");
    return 1;
}

/* exit command */
static int builtin_exit(char **args) {
    int code = args[1] ? atoi(args[1]) : 0;
    exit(code);
    return 0;  /* Not reached */
}

/* help command */
static int builtin_help(char **args) {
    (void)args;
    printf("MyOS Shell\n");
    printf("Built-in commands:\n");
    for (int i = 0; builtins[i].name; i++) {
        printf("  %-8s - %s\n", builtins[i].name, builtins[i].help);
    }
    printf("\nPipe: cmd1 | cmd2\n");
    printf("External commands: Type program name to execute\n");
    return 0;
}

/* kill command */
static int builtin_kill(char **args) {
    if (!args[1]) {
        printf("kill: missing pid\n");
        return 1;
    }

    int sig = 15;  /* SIGTERM default */
    int pid;

    if (args[1][0] == '-') {
        sig = atoi(args[1] + 1);
        if (!args[2]) {
            printf("kill: missing pid\n");
            return 1;
        }
        pid = atoi(args[2]);
    } else {
        pid = atoi(args[1]);
    }

    if (kill(pid, sig) < 0) {
        printf("kill: failed to send signal\n");
        return 1;
    }

    return 0;
}

/* Parse command line into arguments */
static int parse_line(char *line, char **args) {
    int argc = 0;

    while (*line && argc < MAX_ARGS - 1) {
        /* Skip whitespace */
        while (*line == ' ' || *line == '\t') {
            line++;
        }

        if (*line == '\0' || *line == '\n') {
            break;
        }

        /* Check for pipe character */
        if (*line == '|') {
            args[argc++] = line;
            *line++ = '\0';
            continue;
        }

        /* Start of argument */
        args[argc++] = line;

        /* Find end of argument */
        while (*line && *line != ' ' && *line != '\t' && *line != '\n' && *line != '|') {
            line++;
        }

        if (*line && *line != '|') {
            *line++ = '\0';
        }
    }

    args[argc] = NULL;
    return argc;
}

/* Find and execute built-in command */
static int try_builtin(char **args) {
    for (int i = 0; builtins[i].name; i++) {
        if (strcmp(args[0], builtins[i].name) == 0) {
            return builtins[i].handler(args);
        }
    }
    return -1;  /* Not a built-in */
}

/* Execute a single command */
static void exec_cmd(char **args) {
    char path[256];

    /* If command doesn't contain '/', try /bin/ prefix */
    if (strchr(args[0], '/') == NULL) {
        snprintf(path, sizeof(path), "/bin/%s", args[0]);
        execve(path, args, NULL);
    }

    /* Try the command as-is */
    execve(args[0], args, NULL);

    /* Exec failed */
    printf("sh: %s: command not found\n", args[0]);
    exit(127);
}

/* Execute external command (no pipes) */
static int execute(char **args) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0) {
        printf("sh: fork failed\n");
        return 1;
    }

    if (pid == 0) {
        /* Child process */
        exec_cmd(args);
    }

    /* Parent process - wait for child */
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 1;
}

/* Check if args contain a pipe */
static int find_pipe(char **args, int argc) {
    for (int i = 0; i < argc; i++) {
        if (args[i][0] == '|' && args[i][1] == '\0') {
            return i;
        }
    }
    return -1;
}

/* Execute commands with pipe */
static int execute_pipe(char **args, int argc) {
    int pipe_pos = find_pipe(args, argc);

    if (pipe_pos < 0) {
        /* No pipe, execute normally */
        return execute(args);
    }

    /* Split at pipe */
    args[pipe_pos] = NULL;
    char **cmd1 = args;
    char **cmd2 = &args[pipe_pos + 1];

    if (!cmd1[0] || !cmd2[0]) {
        printf("sh: syntax error near '|'\n");
        return 1;
    }

    /* Create pipe */
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        printf("sh: pipe failed\n");
        return 1;
    }

    /* Fork first child (writes to pipe) */
    pid_t pid1 = fork();
    if (pid1 < 0) {
        printf("sh: fork failed\n");
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    if (pid1 == 0) {
        /* First child: redirect stdout to pipe write end */
        close(pipefd[0]);           /* Close unused read end */
        dup2(pipefd[1], 1);         /* stdout -> pipe write */
        close(pipefd[1]);           /* Close original fd */
        exec_cmd(cmd1);
    }

    /* Fork second child (reads from pipe) */
    pid_t pid2 = fork();
    if (pid2 < 0) {
        printf("sh: fork failed\n");
        close(pipefd[0]);
        close(pipefd[1]);
        waitpid(pid1, NULL, 0);
        return 1;
    }

    if (pid2 == 0) {
        /* Second child: redirect stdin from pipe read end */
        close(pipefd[1]);           /* Close unused write end */
        dup2(pipefd[0], 0);         /* stdin <- pipe read */
        close(pipefd[0]);           /* Close original fd */
        exec_cmd(cmd2);
    }

    /* Parent: close both pipe ends and wait for children */
    close(pipefd[0]);
    close(pipefd[1]);

    int status1, status2;
    waitpid(pid1, &status1, 0);
    waitpid(pid2, &status2, 0);

    /* Return status of last command */
    if (WIFEXITED(status2)) {
        return WEXITSTATUS(status2);
    }
    return 1;
}

/* Read a line from stdin */
static char *read_line(char *buf, int size) {
    int i = 0;
    int c;

    while (i < size - 1) {
        c = getchar();
        if (c == EOF) {
            /* No input available - wait and retry */
            /* Small delay to avoid busy loop */
            for (volatile int j = 0; j < 100000; j++);
            continue;
        }
        if (c == '\n') {
            buf[i] = '\0';
            return buf;
        }
        buf[i++] = c;
    }

    buf[i] = '\0';
    return buf;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    char line[MAX_LINE];
    char *args[MAX_ARGS];
    int arg_count;
    int ret;

    printf("\nMyOS Shell v0.2\n");
    printf("Type 'help' for available commands.\n\n");

    while (1) {
        /* Print prompt */
        printf(PROMPT);

        /* Read line */
        if (!read_line(line, sizeof(line))) {
            printf("\n");
            break;
        }

        /* Parse line */
        arg_count = parse_line(line, args);
        if (arg_count == 0) {
            continue;
        }

        /* Check for pipe first (before builtins) */
        if (find_pipe(args, arg_count) >= 0) {
            execute_pipe(args, arg_count);
            continue;
        }

        /* Try built-in first */
        ret = try_builtin(args);
        if (ret >= 0) {
            continue;
        }

        /* Execute external command */
        execute(args);
    }

    return 0;
}
