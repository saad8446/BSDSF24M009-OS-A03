#include "shell.h"

int main() {
    char* cmdline;
    pipeline_t pipeline;

    while ((cmdline = readline(PROMPT)) != NULL) {

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

        // 0 = parsed OK, 1 = empty line, -1 = syntax error (already reported)
        if (parse_pipeline(cmdline, &pipeline) == 0) {
            command_t* first = &pipeline.cmds[0];

            // A single plain command (no pipe, no redirection) may be a
            // built-in that must run inside the shell process itself.
            int plain = (pipeline.n_cmds == 1 && !first->infile && !first->outfile);

            if (!(plain && handle_builtin(first->argv)))
                execute(&pipeline);

            free_pipeline(&pipeline);
        }

        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}