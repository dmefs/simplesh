#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include "run.h"


void run_child(process *p)
{
    int read_fd, write_fd, open_flag;

    read_fd = STDIN_FILENO;
    write_fd = STDOUT_FILENO;

    if (p->input_redirection) {
        if (-1 == (read_fd = open(p->input_redirection, O_RDONLY)))
            goto error_input_redirection;
        if (-1 == dup2(read_fd, STDIN_FILENO))
            goto error_dup_input;
    }
    if (p->output_redirection) {
        open_flag = O_WRONLY | (p->output_option == TRUNC ? O_TRUNC | O_CREAT : O_APPEND);
        if (-1 == (write_fd = open(p->output_redirection, open_flag, 0664))) {
            goto error_output_redirection;
        }
        if (-1 == dup2(write_fd, STDOUT_FILENO))
            goto error_dup_output;
    }
    if ((p->pipe_fd[0] != -1) && !p->input_redirection) {
        dup2(p->pipe_fd[0], STDIN_FILENO);
    }
    if ((p->pipe_fd[1] != -1) && !p->output_redirection) {
        dup2(p->pipe_fd[1], STDOUT_FILENO);
    }

    execvp(p->program_name, p->argument_list);

error_dup_output:
    if (write_fd != STDOUT_FILENO)
        close(write_fd);
error_output_redirection:
error_dup_input:
    if (read_fd != STDIN_FILENO)
        close(read_fd);
error_input_redirection:
    perror("");
    exit(EXIT_FAILURE);
}

pid_t run_process(process *p) {
    pid_t pid;

    if ((pid = fork()) == 0)
        run_child(p);
    return pid;
}

void run_job(job *j) {
    int status;
    int fd[2];

    if (!j)
        return;
    for (process *p = j->process_list; p != NULL; p = p->next) {
        if (p->next) {
            if (-1 == pipe(fd)) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
            p->pipe_fd[1] = fd[1];
            p->next->pipe_fd[0] = fd[0];
        }
        run_process(p);
    }
    wait(&status);
}

void run_jobs(job *j)
{
    for (; j != NULL; j = j->next) {
        run_job(j);
    }
}
