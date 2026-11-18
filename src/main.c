#include "shell.h"

// Splits one input line at ';' and '&', then runs each piece in order.
static void run_line(const char* line) {
    segment_t segs[MAX_SEGMENTS];
    int n = split_line(line, segs, MAX_SEGMENTS);
    if (n < 0) return;                       // syntax error already reported

    for (int i = 0; i < n; i++) {
        pipeline_t pipeline;
        int rc = parse_pipeline(segs[i].text, &pipeline);

        if (rc < 0) break;                   // syntax error: stop the chain
        if (rc > 0) continue;                // empty piece

        command_t* first = &pipeline.cmds[0];

        // A single plain foreground command (no pipe, no redirection) may be
        // a built-in that must run inside the shell process itself.
        int plain = (pipeline.n_cmds == 1 && !first->infile && !first->outfile);

        if (!(plain && !segs[i].background && handle_builtin(first->argv)))
            execute(&pipeline, segs[i].background, segs[i].text);

        free_pipeline(&pipeline);
    }

    free_segments(segs, n);
}

int main() {
    char* cmdline;

    while (1) {
        reap_jobs();                         // collect finished background jobs
                                             // BEFORE showing the prompt

        cmdline = readline(PROMPT);
        if (cmdline == NULL) break;          // Ctrl+D

        reap_jobs();                         // jobs that finished while typing

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
        run_line(cmdline);
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}