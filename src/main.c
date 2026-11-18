#include "shell.h"

// Splits one input line at ';' and '&', then runs each piece in order.
// Returns the exit status of the last command that ran.
int run_line(const char* line) {
    segment_t segs[MAX_SEGMENTS];
    int status = 0;

    int n = split_line(line, segs, MAX_SEGMENTS);
    if (n < 0) {                             // syntax error already reported
        last_exit_status = 1;
        return 1;
    }

    for (int i = 0; i < n; i++) {
        pipeline_t pipeline;
        int rc = parse_pipeline(segs[i].text, &pipeline);

        if (rc < 0) {                        // syntax error: stop the chain
            status = 1;
            last_exit_status = 1;
            break;
        }
        if (rc > 0) continue;                // empty piece

        // Replace $NAME and $? by their values BEFORE running anything,
        // so built-ins and external commands both get the real text.
        expand_pipeline(&pipeline);

        command_t* first = &pipeline.cmds[0];

        // A single plain foreground command (no pipe, no redirection) may be
        // a built-in (or a variable assignment) that must run inside the
        // shell process itself.
        int plain = (pipeline.n_cmds == 1 && !first->infile && !first->outfile);

        if (plain && !segs[i].background && handle_builtin(first->argv))
            status = last_exit_status;       // built-in: status set by itself
        else
            status = execute(&pipeline, segs[i].background, segs[i].text);

        last_exit_status = status;           // remembered for $?

        free_pipeline(&pipeline);
    }

    free_segments(segs, n);
    return status;
}

int main() {
    char* cmdline;

    // Pasted multi-line text should behave like typing line by line
    rl_variable_bind("enable-bracketed-paste", "off");

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

        if (is_blank_or_comment(cmdline)) {
            // nothing to do
        }
        else if (starts_with_word(cmdline, "if")) {
            run_if_statement(cmdline);       // multi-line if ... fi
        }
        else if (starts_with_word(cmdline, "then") ||
                 starts_with_word(cmdline, "else") ||
                 starts_with_word(cmdline, "fi")) {
            fprintf(stderr, "syntax error: unexpected keyword (no matching 'if')\n");
        }
        else {
            run_line(cmdline);
        }

        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}