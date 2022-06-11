#include "job.h"

void command_fg() {
    continue_job(first_job, 1);
}

void command_bg() {
    continue_job(first_job, 0);
}
