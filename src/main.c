#include "shell.h"

int main() {
    char* cmdline;
    char** arglist;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {

        // Handle !n: replace the line with the n-th history command
        if (cmdline[0] == '!') {
            char* endp;
            long n = strtol(cmdline + 1, &endp, 10);

            if (endp == cmdline + 1 || *endp != '\0') {
                fprintf(stderr, "history: invalid syntax, use !n (e.g. !3)\n");
                free(cmdline);
                continue;
            }

            char* entry = get_history_entry((int) n);
            if (entry == NULL) {
                fprintf(stderr, "history: no such command: %ld\n", n);
                free(cmdline);
                continue;
            }

            free(cmdline);
            cmdline = strdup(entry);   // run the stored command
            printf("%s\n", cmdline);   // show what is being re-executed
        }

        add_history_entry(cmdline);

        if ((arglist = tokenize(cmdline)) != NULL) {
            if (handle_builtin(arglist) == 0)
                execute(arglist);

            // Free the memory allocated by tokenize()
            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}