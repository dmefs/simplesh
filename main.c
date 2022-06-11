#include <string.h>
#include "parse.h"
#include <termios.h>
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include "run.h"
#include "job.h"
#include "command.h"

void print_job_list(job*);

    const struct command commands[] = {
        {.name = "fg\n"     , .func = command_fg     },
        {.name = "bg\n"     , .func = command_bg     },
        {NULL, NULL}, // don't forget this line, which indicates tail.
    };

int main(int argc, char *argv[]) {
    char s[LINELEN];
    job *curr_job;
    bool is_default_command;
    

    init_shell();

    while(get_line(s, LINELEN)) {
        is_default_command = false;

        if(!strcmp(s, "exit\n"))
            break;
        signal(SIGCHLD, SIG_BLOCK);
        for (const struct command *c = commands; c->name; c++) {
            if (!strcmp(s, c->name)) {
                is_default_command = true;
                c->func();
            }
        }
        signal(SIGCHLD, sigchld_handler);
        if (is_default_command)
            continue;
        curr_job = parse_line(s);

        if (curr_job)
            add_job(curr_job);
        run_jobs(curr_job);
    }

    return 0;
}
