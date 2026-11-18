#include "shell.h"

// Exit status of the most recent built-in command
int last_exit_status = 0;

// ---------- History storage ----------
static char* cmd_history[HISTORY_SIZE];
static int hist_count = 0;

void add_history_entry(const char* cmdline) {
    if (cmdline == NULL) return;

    // Skip blank lines (only spaces/tabs)
    const char* p = cmdline;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0') return;

    // Also remember it in readline so the arrow keys can recall it
    add_history(cmdline);

    // If full, drop the oldest entry and shift the rest down
    if (hist_count == HISTORY_SIZE) {
        free(cmd_history[0]);
        for (int i = 1; i < HISTORY_SIZE; i++)
            cmd_history[i - 1] = cmd_history[i];
        hist_count--;
    }

    cmd_history[hist_count++] = strdup(cmdline);
}

void print_history(void) {
    for (int i = 0; i < hist_count; i++)
        printf("%3d  %s\n", i + 1, cmd_history[i]);
}

// n is 1-based. Returns NULL if n is out of range.
char* get_history_entry(int n) {
    if (n < 1 || n > hist_count) return NULL;
    return cmd_history[n - 1];
}

// ---------- Parsing ----------
static int is_space(char c) { return c == ' ' || c == '\t'; }
static int is_op(char c)    { return c == '|' || c == '<' || c == '>'; }

void free_segments(segment_t segs[], int n) {
    for (int i = 0; i < n; i++)
        free(segs[i].text);
}

// Splits a line at ';' and '&' (ignoring any inside quotes).
// A segment ended by '&' is marked as a background command.
// Returns the number of segments, or -1 on a syntax error
// (the message is already printed).
int split_line(const char* line, segment_t segs[], int max) {
    int n = 0;
    const char* start = line;
    char quote = 0;

    for (const char* cp = line; ; cp++) {
        char c = *cp;
        int at_end = (c == '\0');
        int bg = 0;

        if (!at_end) {
            if (quote) {                         // inside quotes: no splitting
                if (c == quote) quote = 0;
                continue;
            }
            if (c == '"' || c == '\'') { quote = c; continue; }

            if (c == '&') {
                if (cp[1] == '&') {
                    fprintf(stderr, "syntax error: '&&' is not supported\n");
                    goto error;
                }
                bg = 1;
            } else if (c != ';') {
                continue;                        // ordinary character
            }
        }

        // The segment is [start, cp). Trim spaces at both ends.
        const char* s = start;
        const char* e = cp;
        while (s < e && is_space(*s)) s++;
        while (e > s && is_space(e[-1])) e--;
        size_t len = (size_t)(e - s);

        if (len == 0) {
            if (bg) {
                fprintf(stderr, "syntax error: missing command before '&'\n");
                goto error;
            }
            // empty piece (e.g. a trailing ';') -- just skip it
        } else {
            if (n == max) {
                fprintf(stderr, "error: too many commands on one line (max %d)\n", max);
                goto error;
            }
            segs[n].text = strndup(s, len);
            segs[n].background = bg;
            n++;
        }

        if (at_end) break;
        start = cp + 1;
    }
    return n;

error:
    free_segments(segs, n);
    return -1;
}

// Reads one word starting at *pp. Stops at whitespace or an operator
// (unless inside quotes). Quote characters are removed from the result.
// Advances *pp past the word. Returns NULL on an unterminated quote.
static char* read_word(const char** pp) {
    const char* cp = *pp;
    char* buf = malloc(strlen(cp) + 1);
    size_t len = 0;
    char quote = 0;

    while (*cp != '\0') {
        if (quote) {
            if (*cp == quote) quote = 0;       // closing quote
            else buf[len++] = *cp;
        } else if (*cp == '"' || *cp == '\'') {
            quote = *cp;                       // opening quote
        } else if (is_space(*cp) || is_op(*cp)) {
            break;                             // end of word
        } else {
            buf[len++] = *cp;
        }
        cp++;
    }

    *pp = cp;
    if (quote) {                               // quote never closed
        free(buf);
        return NULL;
    }
    buf[len] = '\0';
    return buf;
}

void free_pipeline(pipeline_t* p) {
    for (int i = 0; i < p->n_cmds; i++) {
        for (int j = 0; j < p->cmds[i].argc; j++)
            free(p->cmds[i].argv[j]);
        free(p->cmds[i].infile);
        free(p->cmds[i].outfile);
    }
    p->n_cmds = 0;
}

// Returns 0 on success, 1 for an empty line, -1 on a syntax error
// (the error message is already printed).
int parse_pipeline(const char* line, pipeline_t* p) {
    memset(p, 0, sizeof(*p));
    p->n_cmds = 1;
    command_t* cur = &p->cmds[0];
    const char* cp = line;

    while (*cp != '\0') {
        while (is_space(*cp)) cp++;
        if (*cp == '\0') break;

        if (*cp == '|') {
            if (cur->argc == 0) {
                fprintf(stderr, "syntax error: missing command before '|'\n");
                goto error;
            }
            if (p->n_cmds == MAX_CMDS) {
                fprintf(stderr, "error: too many commands in pipeline (max %d)\n", MAX_CMDS);
                goto error;
            }
            cp++;
            cur = &p->cmds[p->n_cmds++];
        }
        else if (*cp == '<' || *cp == '>') {
            char op = *cp++;
            while (is_space(*cp)) cp++;
            if (*cp == '\0' || is_op(*cp)) {
                fprintf(stderr, "syntax error: expected a filename after '%c'\n", op);
                goto error;
            }
            char* name = read_word(&cp);
            if (name == NULL) {
                fprintf(stderr, "syntax error: unterminated quote\n");
                goto error;
            }
            if (op == '<') { free(cur->infile);  cur->infile  = name; }
            else           { free(cur->outfile); cur->outfile = name; }
        }
        else {
            if (cur->argc >= MAXARGS) {
                fprintf(stderr, "error: too many arguments (max %d)\n", MAXARGS);
                goto error;
            }
            char* word = read_word(&cp);
            if (word == NULL) {
                fprintf(stderr, "syntax error: unterminated quote\n");
                goto error;
            }
            cur->argv[cur->argc++] = word;
        }
    }

    // Completely empty line
    if (p->n_cmds == 1 && cur->argc == 0 && !cur->infile && !cur->outfile)
        return 1;

    // Line ended with '|' or had only redirections and no command
    if (cur->argc == 0) {
        fprintf(stderr, "syntax error: missing command\n");
        goto error;
    }

    return 0;

error:
    free_pipeline(p);
    return -1;
}

// ---------- Built-ins ----------
// Returns 1 if the command was a built-in (already handled), 0 otherwise.
// A built-in stores its exit status in last_exit_status.
int handle_builtin(char** arglist) {
    if (arglist == NULL || arglist[0] == NULL)
        return 0;

    if (strcmp(arglist[0], "exit") == 0) {
        printf("Shell exited.\n");
        exit(0);
    }

    if (strcmp(arglist[0], "cd") == 0) {
        last_exit_status = 0;
        if (arglist[1] == NULL) {
            fprintf(stderr, "cd: missing directory\n");
            last_exit_status = 1;
        } else if (chdir(arglist[1]) != 0) {
            perror("cd");
            last_exit_status = 1;
        }
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  exit        Exit the shell\n");
        printf("  cd <dir>    Change the current directory\n");
        printf("  help        Show this help message\n");
        printf("  jobs        List running background jobs\n");
        printf("  history     Show the last %d commands\n", HISTORY_SIZE);
        printf("  !n          Re-run command number n from history\n");
        printf("Operators:\n");
        printf("  cmd < file, cmd > file   Redirect input / output\n");
        printf("  cmd1 | cmd2              Pipe\n");
        printf("  cmd1 ; cmd2              Run one after another\n");
        printf("  cmd &                    Run in the background\n");
        printf("Control structure (then/else/fi each on its own line):\n");
        printf("  if <command>\n");
        printf("  then\n");
        printf("    <commands>\n");
        printf("  else\n");
        printf("    <commands>\n");
        printf("  fi\n");
        last_exit_status = 0;
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        print_jobs();
        last_exit_status = 0;
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        print_history();
        last_exit_status = 0;
        return 1;
    }

    return 0;
}