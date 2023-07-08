#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>

#include "parse.h"

static void redirect(int oldfd, int newfd) {
    if (oldfd != newfd) {
        if (dup2(oldfd, newfd) != -1) {
            close(oldfd); /* successfully redirected */
        } else {
            perror("dup2");
            exit(1);
        }
    }
}

int run_single_command(struct command *command) {
    // printf("Running command: %s\n", command->argv[0]);
    if (command->input_redir) {
        int fd = open(command->input_redir, O_RDONLY);
        if (fd == -1) {
            perror("open");
            exit(1);
        }
        redirect(fd, STDIN_FILENO);
    }
    if (command->output_redir) {
        int fd =
            open(command->output_redir, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd == -1) {
            perror("open");
            exit(1);
        }
        redirect(fd, STDOUT_FILENO);
    }
    if (execvp(command->argv[0], command->argv) == -1) {
        perror("execv");
        exit(1);
    }
    return 0;
}

int run_recurisive(struct command *command, int fd_in) {
    if (command->next == NULL) {
        // the last command does not output to a pipe
        redirect(fd_in, STDIN_FILENO);
        return run_single_command(command);
    }

    // pipe_fd[1] ==> pipe_fd[0]
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(1);
    }
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    } else if (pid == 0) {
        // child: execute current command
        // fd_in ==> command ==> pipe_fd[1] ==> pipe_fd[0]
        close(pipe_fd[0]);
        redirect(fd_in, STDIN_FILENO);
        redirect(pipe_fd[1], STDOUT_FILENO);
        return run_single_command(command);
    } else {
        // parent: process remaining commands
        // read from pipe_fd[0]
        close(pipe_fd[1]);
        close(fd_in);
        return run_recurisive(command->next, pipe_fd[0]);
    }
    return 0;
}

/* TODO: implement this */
void run_pipeline(struct pipeline *p) {
    struct command *cmd = &p->first_command;
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    } else if (pid == 0) {
        // child: execute the commands
        run_recurisive(cmd, STDIN_FILENO);
    } else {
        // parent: wait for child
        if (!p->background) {
            int status;
            wait(&status);
        }
    }
}

/* TODO: implement this */
void run_builtin(enum builtin_type builtin, char *builtin_arg) {
    // printf("run_builtin: %d, %s\n", builtin, builtin_arg);
    if (builtin == BUILTIN_EXIT) {
        int ret = builtin_arg ? atoi(builtin_arg) : 0;
        exit(ret);
    } else if (builtin == BUILTIN_WAIT) {
        pid_t pid = builtin_arg ? atoi(builtin_arg) : -1;
        waitpid(pid, NULL, 0);
    } else if (builtin == BUILTIN_KILL) {
        if (builtin_arg == NULL) {
            fprintf(stderr, "kill: missing argument\n");
            exit(1);
        }
        int status;
        kill(atoi(builtin_arg), SIGTERM);
        wait(&status);
    }
}
