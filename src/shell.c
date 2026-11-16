#include "shell.h"

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
char** tokenize(char* cmdline) {
    // Edge case: empty command line
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)malloc(sizeof(char) * ARGLEN);
        bzero(arglist[i], ARGLEN);
    }

    char* cp = cmdline;
    char* start;
    int len;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++; // Skip leading whitespace
        
        if (*cp == '\0') break; // Line was only whitespace

        start = cp;
        len = 1;
        while (*++cp != '\0' && !(*cp == ' ' || *cp == '\t')) {
            len++;
        }
        if (len > ARGLEN - 1) len = ARGLEN - 1;   // never overflow the buffer
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) { // No arguments were parsed
        for(int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
}

// ---------- Built-ins ----------
// Returns 1 if the command was a built-in (already handled), 0 otherwise
int handle_builtin(char** arglist) {
    if (arglist == NULL || arglist[0] == NULL)
        return 0;

    if (strcmp(arglist[0], "exit") == 0) {
        printf("Shell exited.\n");
        exit(0);
    }

    if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] == NULL) {
            fprintf(stderr, "cd: missing directory\n");
        } else if (chdir(arglist[1]) != 0) {
            perror("cd");
        }
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  exit        Exit the shell\n");
        printf("  cd <dir>    Change the current directory\n");
        printf("  help        Show this help message\n");
        printf("  jobs        List background jobs (not yet implemented)\n");
        printf("  history     Show the last %d commands\n", HISTORY_SIZE);
        printf("  !n          Re-run command number n from history\n");
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        print_history();
        return 1;
    }

    return 0;
}