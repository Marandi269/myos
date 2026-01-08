/*
 * sh.c - Simple shell
 *
 * A basic command-line interpreter supporting:
 * - Built-in commands: cd, pwd, exit, help
 * - External command execution
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
#define PROMPT "$ "

/* Built-in command handlers */
static int builtin_cd(char **args);
static int builtin_pwd(char **args);
static int builtin_exit(char **args);
static int builtin_help(char **args);

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
    printf("\nExternal commands: Type program name to execute\n");
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

        /* Start of argument */
        args[argc++] = line;

        /* Find end of argument */
        while (*line && *line != ' ' && *line != '\t' && *line != '\n') {
            line++;
        }

        if (*line) {
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

/* Execute external command */
static int execute(char **args) {
    pid_t pid;
    int status;
    char path[256];

    pid = fork();
    if (pid < 0) {
        printf("sh: fork failed\n");
        return 1;
    }

    if (pid == 0) {
        /* Child process */

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

    /* Parent process - wait for child */
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 1;
}

/* Read a line from stdin */
static char *read_line(char *buf, int size) {
    int i = 0;
    int c;

    while (i < size - 1) {
        c = getchar();
        if (c == EOF || c == '\n') {
            buf[i] = '\0';
            return (i > 0 || c == '\n') ? buf : NULL;
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

    printf("\nMyOS Shell v0.1\n");
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
