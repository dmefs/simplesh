#include "job.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

job* first_job = NULL;
pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;

void sigchld_handler(int sig) {
    do_job_notification();
}

void
init_shell()
{
    /* See if we are running interactively.  */
    shell_terminal = STDIN_FILENO;
    shell_is_interactive = isatty(shell_terminal);

    printf("shell term = %d\n", shell_terminal);
    printf("shell term pgid = %d\n", tcgetpgrp(shell_terminal));
    printf("shell_pgid = %d\n", getpgrp());
    if (shell_is_interactive) {
        /* Loop until we are in the foreground.  */
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);

        /* Ignore interactive and job-control signals.  */
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        //signal(SIGCHLD, SIG_IGN);
        signal(SIGCHLD, sigchld_handler);

        /* Put ourselves in our own process group.  */
        shell_pgid = getpid();
        if (setpgid(shell_pgid, shell_pgid) < 0) {
            perror("Couldn't put the shell in its own process group");
            exit(EXIT_FAILURE);
        }

        /* Grab control of the terminal.  */
        tcsetpgrp(shell_terminal, shell_pgid);

        /* Save default terminal attributes for shell.  */
        tcgetattr(shell_terminal, &shell_tmodes);
    }
}

job*
find_job(pid_t pgid)
{
    job* j;

    for (j = first_job; j; j = j->next)
        if (j->pgid == pgid)
            return j;
    return NULL;
}

int
job_is_stopped(job* j)
{
    process* p;

    for (p = j->process_list; p; p = p->next)
        if (!p->completed && !p->stopped)
            return 0;
    return 1;
}

int
job_is_completed(job* j)
{
    process* p;

    for (p = j->process_list; p; p = p->next)
        if (!p->completed)
            return 0;
    return 1;
}

void
put_job_in_foreground(job* j, int cont)
{
    /* Put the job into the foreground.  */
    tcsetpgrp(shell_terminal, j->pgid);

    /* Send the job a continue signal, if necessary.  */
    if (cont) {
        tcsetattr(shell_terminal, TCSADRAIN, &j->tmodes);
        if (kill(-j->pgid, SIGCONT) < 0)
            perror("kill (SIGCONT)");
    }

    /* Wait for it to report.  */
    wait_for_job(j);

    /* Put the shell back in the foreground.  */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Restore the shell’s terminal modes.  */
    tcgetattr(shell_terminal, &j->tmodes);
    tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);
}

void
put_job_in_background(job* j, int cont)
{
    /* Send the job a continue signal, if necessary.  */
    if (cont)
        if (kill(-j->pgid, SIGCONT) < 0)
            perror("kill (SIGCONT)");
}

/* Store the status of the process pid that was returned by waitpid.
   Return 0 if all went well, nonzero otherwise.  */

int
mark_process_status(pid_t pid, int status)
{
    job* j;
    process* p;

    if (pid > 0) {
        /* Update the record for the process.  */
        for (j = first_job; j; j = j->next) {
            for (p = j->process_list; p; p = p->next)
                if (p->pid == pid) {
                    p->status = status;
                    if (WIFSTOPPED(status))
                        p->stopped = 1;
                    else {
                        p->completed = 1;
                        if (WIFSIGNALED(status))
                            fprintf(stderr, "%d: Terminated by signal %d.\n", (int)pid, WTERMSIG(p->status));
                    }
                    return 0;
                }
        }
        fprintf(stderr, "No child process %d.\n", pid);
        return -1;
    } else if (pid == 0 || errno == ECHILD)
        /* No processes ready to report.  */
        return -1;
    else {
        /* Other weird errors.  */
        perror("waitpid");
        return -1;
    }
}

/* Check for processes that have status information available,
   without blocking.  */

void
update_status(void)
{
    int status;
    pid_t pid;

    do
        pid = waitpid(WAIT_ANY, &status, WUNTRACED | WNOHANG);
    while (!mark_process_status(pid, status));
}

/* Check for processes that have status information available,
   blocking until all processes in the given job have reported.  */

void
wait_for_job(job* j)
{
    int status;
    pid_t pid;

    do
        pid = waitpid(WAIT_ANY, &status, WUNTRACED);
    while (!mark_process_status(pid, status) && !job_is_stopped(j) && !job_is_completed(j));
}

/* Format information about job status for the user to look at.  */

void
format_job_info(job* j, const char* status)
{
    fprintf(stderr, "%ld (%s): %s\n", (long)j->pgid, status, j->command);
}

/* Notify the user about stopped or terminated jobs.
   Delete terminated jobs from the active job list.  */

void
do_job_notification(void)
{
    job *j, *jlast, *jnext;

    /* Update status information for child processes.  */
    update_status();

    jlast = NULL;
    for (j = first_job; j; j = jnext) {
        jnext = j->next;

        /* If all processes have completed, tell the user the job has
         completed and delete it from the list of active jobs.  */
        if (job_is_completed(j)) {
            format_job_info(j, "completed");
            if (jlast)
                jlast->next = jnext;
            else
                first_job = jnext;
            free_job(j);
        }

        /* Notify the user about stopped jobs,
         marking them so that we won’t do this more than once.  */
        else if (job_is_stopped(j) && !j->notified) {
            format_job_info(j, "stopped");
            j->notified = 1;
            jlast = j;
        }

        /* Don’t say anything about jobs that are still running.  */
        else
            jlast = j;
    }
}

/* Mark a stopped job J as being running again.  */

void
mark_job_as_running (job *j)
{
  process *p;

  for (p = j->process_list; p; p = p->next)
    p->stopped = 0;
  j->notified = 0;
}

/* Continue the job J.  */

void
continue_job (job *j, int foreground)
{
  mark_job_as_running (j);
  if (foreground)
    put_job_in_foreground (j, 1);
  else
    put_job_in_background (j, 1);
}