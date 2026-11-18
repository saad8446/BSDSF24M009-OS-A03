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
#define PROMPT "FCIT> "
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

// Parsing (shell.c)
int  parse_pipeline(const char* line, pipeline_t* p);
void free_pipeline(pipeline_t* p);

// Execution (execute.c)
int execute(pipeline_t* p);

// Built-ins (shell.c)
int handle_builtin(char** arglist);

// History (shell.c)
void add_history_entry(const char* cmdline);
void print_history(void);
char* get_history_entry(int n);

#endif // SHELL_H