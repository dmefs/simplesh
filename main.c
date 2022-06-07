#include <string.h>
#include "parse.h"
#include "run.h"

void print_job_list(job*);

int main(int argc, char *argv[]) {
    char s[LINELEN];
    job *curr_job;

    while(get_line(s, LINELEN)) {
        if(!strcmp(s, "exit\n"))
            break;

        curr_job = parse_line(s);

        run_job(curr_job);

        free_job(curr_job);
    }

    return 0;
}
