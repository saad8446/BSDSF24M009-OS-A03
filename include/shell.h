#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_LEN 512
#define MAXARGS 10
#define MAX_CMDS 10
#define MAX_SEGMENTS 32
#define MAX_JOBS 32
#define PROMPT  "FCIT> "
#define PROMPT2 "> "            // continuation prompt inside an if block
#define HISTORY_SIZE 20

// One command: its arguments plus optional < and > files
typedef struct {
    char* argv[MAXARGS + 1];
    int   argc;
    char* infile;
    char* outfile;
} command_t;

// A pipeline: commands separated by |
typedef struct {
    command_t cmds[MAX_CMDS];
    int       n_cmds;
} pipeline_t;

// One piece of a line that was split at ';' or '&'
typedef struct {
    char* text;        // the command text (trimmed)
    int   background;  // 1 if it was terminated by '&'
} segment_t;

// Exit status of the most recent built-in (0 = success)
extern int last_exit_status;

// Parsing (shell.c)
int  split_line(const char* line, segment_t segs[], int max);
void free_segments(segment_t segs[], int n);
int  parse_pipeline(const char* line, pipeline_t* p);
void free_pipeline(pipeline_t* p);

// Execution (execute.c)
int execute(pipeline_t* p, int background, const char* cmdtext);

// Runs one line (handles ; & | < >) and returns the exit status of the
// last command it ran (main.c)
int run_line(const char* line);

// Built-ins (shell.c)
int handle_builtin(char** arglist);

// History (shell.c)
void add_history_entry(const char* cmdline);
void print_history(void);
char* get_history_entry(int n);

// Background jobs (jobs.c)
void add_job(pid_t pid, const char* cmdtext);
void reap_jobs(void);
void print_jobs(void);

// if-then-else-fi (ifelse.c)
int  starts_with_word(const char* line, const char* word);
int  is_blank_or_comment(const char* line);
void run_if_statement(const char* first_line);

#endif // SHELL_H