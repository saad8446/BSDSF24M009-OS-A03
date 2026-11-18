#include "shell.h"
#include <fcntl.h>

// Runs in the CHILD, after fork() and before exec().
// Rewires stdin/stdout to the files named with < and >.
static void setup_redirections(command_t* cmd) {
    if (cmd->infile != NULL) {
        int fd = open(cmd->infile, O_RDONLY);
        if (fd < 0) {
            perror(cmd->infile);
            exit(1);
        }
        dup2(fd, STDIN_FILENO);    // fd 0 now reads from the file
        close(fd);                 // original fd no longer needed
    }

    if (cmd->outfile != NULL) {
        int fd = open(cmd->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror(cmd->outfile);
            exit(1);
        }
        dup2(fd, STDOUT_FILENO);   // fd 1 now writes to the file
        close(fd);
    }
}

// Runs every command of the pipeline, connecting them with pipes.
// Foreground: waits for all children and returns the last exit status.
// Background: does NOT wait; records the job and returns 0 at once.
int execute(pipeline_t* p, int background, const char* cmdtext) {
    int n = p->n_cmds;
    int fd[2];                 // the pipe being created for this stage
    int prev_fd = -1;          // read end of the previous stage's pipe
    pid_t pids[MAX_CMDS];
    int launched = 0;

    fflush(stdout);            // don't let children inherit unflushed output

    for (int i = 0; i < n; i++) {
        int is_last = (i == n - 1);

        if (!is_last && pipe(fd) < 0) {
            perror("pipe");
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            if (!is_last) { close(fd[0]); close(fd[1]); }
            break;
        }

        if (pid == 0) {
            // ----- CHILD -----
            if (prev_fd != -1) {                   // read from previous pipe
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            } else if (background && p->cmds[0].infile == NULL) {
                // A background job must not read the keyboard (the shell
                // needs it), so give it an empty input instead.
                int null_fd = open("/dev/null", O_RDONLY);
                if (null_fd >= 0) {
                    dup2(null_fd, STDIN_FILENO);
                    close(null_fd);
                }
            }
            if (!is_last) {                        // write into the next pipe
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
            }

            setup_redirections(&p->cmds[i]);       // < and > override the pipe

            // A built-in inside a pipe or redirection runs here, in the child
            if (handle_builtin(p->cmds[i].argv))
                exit(last_exit_status);            // CHANGED: real status

            execvp(p->cmds[i].argv[0], p->cmds[i].argv);
            perror("Command not found");           // only runs if exec failed
            exit(127);                             // CHANGED: 127 = not found
        }

        // ----- PARENT -----
        pids[launched++] = pid;

        if (prev_fd != -1) close(prev_fd);         // done with the old read end
        prev_fd = -1;
        if (!is_last) {
            close(fd[1]);                          // parent never writes
            prev_fd = fd[0];                       // keep read end for next child
        }
    }

    if (prev_fd != -1) close(prev_fd);             // cleanup if we broke out early

    // Background: do not wait. Remember the job and return to the prompt.
    if (background) {
        if (launched > 0)
            add_job(pids[launched - 1], cmdtext);
        return 0;
    }

    // Foreground: wait for every child so none become zombies
    int status = 0, last_status = 0;
    for (int i = 0; i < launched; i++) {
        waitpid(pids[i], &status, 0);
        if (i == launched - 1)
            last_status = WIFEXITED(status) ? WEXITSTATUS(status)
                                            : 128 + WTERMSIG(status);
    }
    return last_status;
}