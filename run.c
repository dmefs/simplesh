#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "run.h"

void run_job(job *j)
{
    pid_t pid;
    int status;

    for (; j != NULL; j = j->next) {
        for (process *p = j->process_list; p != NULL; p = p->next) {
            if ((pid = fork()) == 0) {
                /* child process */
                execvp(p->program_name, p->argument_list);
                printf("%s\n", strerror(errno));
            } else {
                waitpid(pid, &status, WUNTRACED);
            }
        }
    }
}
