#include "shell.h"

typedef struct {
    int   id;              // job number shown to the user: [1], [2], ...
    pid_t pid;             // process id of the (last) process of the job
    char  cmd[MAX_LEN];    // the command text, for display
} job_t;

static job_t jobs[MAX_JOBS];
static int job_count = 0;
static int next_job_id = 1;

// Remember a newly started background job and announce it.
void add_job(pid_t pid, const char* cmdtext) {
    if (job_count == MAX_JOBS) {
        fprintf(stderr, "warning: job table full, job %d is not tracked\n", (int) pid);
        return;
    }
    if (job_count == 0)
        next_job_id = 1;                 // restart numbering when list is empty

    job_t* j = &jobs[job_count++];
    j->id  = next_job_id++;
    j->pid = pid;
    strncpy(j->cmd, cmdtext, MAX_LEN - 1);
    j->cmd[MAX_LEN - 1] = '\0';

    printf("[%d] %d\n", j->id, (int) pid);
}

// Collect every finished child WITHOUT blocking, so no zombies are left.
void reap_jobs(void) {
    int status;
    pid_t pid;

    // WNOHANG: return 0 immediately if no child has finished yet
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].pid == pid) {
                printf("[%d]+ Done    %s\n", jobs[i].id, jobs[i].cmd);
                // remove it from the list by shifting the rest down
                for (int k = i + 1; k < job_count; k++)
                    jobs[k - 1] = jobs[k];
                job_count--;
                break;
            }
        }
    }
}

// The 'jobs' built-in: list background jobs that are still running.
void print_jobs(void) {
    reap_jobs();                         // drop finished jobs first

    if (job_count == 0) {
        printf("No background jobs.\n");
        return;
    }
    for (int i = 0; i < job_count; i++)
        printf("[%d]  %d  Running  %s &\n", jobs[i].id, (int) jobs[i].pid, jobs[i].cmd);
}