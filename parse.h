#ifndef __PARSE_H__
#define __PARSE_H__

#include <sys/types.h>
#include <unistd.h>
#include <termios.h>

#define PROMPT "ish$ " /* 入力ライン冒頭の文字列 */
#define NAMELEN 32    /* 各種名前の長さ */
#define ARGLSTLEN 16  /* 1つのプロセスがとる実行時引数の数 */
#define LINELEN 256   /* 入力コマンドの長さ */

typedef enum write_option_ {
    TRUNC,
    APPEND,
} write_option;

typedef struct process_ {
    char*        program_name;
    char**       argument_list;

    char*        input_redirection;

    write_option output_option;
    char*        output_redirection;
    struct process_* next;
    int          pipe_fd[2];
    int          pipe_operation[2];
    int          status;
    pid_t        pid;
    char         completed;
    char         stopped;
} process;

typedef enum job_mode_ {
    FOREGROUND,
    BACKGROUND,
} job_mode;

typedef struct job_ {
    job_mode     mode;
    process*     process_list;
    struct job_* next;
    char*        command;
    pid_t        pgid;
    char         notified;
    struct termios tmodes;
} job;

typedef enum parse_state_ {
    ARGUMENT,
    IN_REDIRCT,
    OUT_REDIRCT_TRUNC,
    OUT_REDIRCT_APPEND,
} parse_state;

char* get_line(char *, int);
job* parse_line(char *);
void free_job(job *);

#endif
