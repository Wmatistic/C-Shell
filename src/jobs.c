#include "mysh.h"

static int next_job_id = 1;

job_t *add_job(pid_t pgid, const char *cmdline, job_status_t status)
{
    job_t *j    = xmalloc(sizeof(job_t));
    j->id       = next_job_id++;
    j->pgid     = pgid;
    j->cmdline  = xstrdup(cmdline);
    j->status   = status;
    j->notified = 0;
    j->next     = job_list;
    job_list    = j;
    return j;
}

void remove_job(int id)
{
    job_t **pp = &job_list;
    while (*pp) {
        if ((*pp)->id == id) {
            job_t *dead = *pp;
            *pp = dead->next;
            free(dead->cmdline);
            free(dead);
            return;
        }
        pp = &(*pp)->next;
    }
}

job_t *find_job_by_id(int id)
{
    for (job_t *j = job_list; j; j = j->next)
        if (j->id == id) return j;
    return NULL;
}

job_t *find_job_by_pgid(pid_t pgid)
{
    for (job_t *j = job_list; j; j = j->next)
        if (j->pgid == pgid) return j;
    return NULL;
}

/*
 * Called from the SIGCHLD handler. Save/restore errno because signal
 * handlers can fire between any two instructions.
 */
void sigchld_handler(int sig)
{
    (void)sig;
    int   saved = errno;
    int   ws;
    pid_t pid;

    while ((pid = waitpid(-1, &ws, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        job_t *j = find_job_by_pgid(pid);
        if (!j) {
            for (job_t *jj = job_list; jj; jj = jj->next)
                if (jj->pgid == getpgid(pid)) { j = jj; break; }
        }
        if (!j) continue;

        if (WIFSTOPPED(ws)) {
            j->status = JOB_STOPPED;
        } else if (WIFCONTINUED(ws)) {
            j->status = JOB_RUNNING;
        } else if (WIFEXITED(ws) || WIFSIGNALED(ws)) {
            j->status = JOB_DONE;
            last_exit_status = WIFEXITED(ws) ? WEXITSTATUS(ws)
                                             : 128 + WTERMSIG(ws);
        }
    }
    errno = saved;
}

void notify_jobs(void)
{
    job_t *j = job_list;
    while (j) {
        job_t *next = j->next;
        if (j->status == JOB_DONE && !j->notified) {
            printf("[%d]+ Done\t\t%s\n", j->id, j->cmdline);
            remove_job(j->id);
        } else if (j->status == JOB_STOPPED && !j->notified) {
            printf("[%d]+ Stopped\t\t%s\n", j->id, j->cmdline);
            j->notified = 1;
        }
        j = next;
    }
}

void print_jobs(void)
{
    for (job_t *j = job_list; j; j = j->next) {
        const char *state = j->status == JOB_RUNNING ? "Running"
                          : j->status == JOB_STOPPED ? "Stopped" : "Done";
        printf("[%d]  %-10s %s\n", j->id, state, j->cmdline);
    }
}

void wait_for_job(job_t *job)
{
    sigset_t prev, chld;
    sigemptyset(&chld);
    sigaddset(&chld, SIGCHLD);

    /* Block SIGCHLD before the status check to avoid a race where the
     * handler updates job->status between the check and sigsuspend. */
    sigprocmask(SIG_BLOCK, &chld, &prev);

    while (job->status == JOB_RUNNING)
        sigsuspend(&prev);  /* atomically unblock SIGCHLD and sleep */

    sigprocmask(SIG_SETMASK, &prev, NULL);

    if (job->status == JOB_STOPPED) {
        printf("\n[%d]+ Stopped\t\t%s\n", job->id, job->cmdline);
        job->notified = 1;
        return;
    }

    remove_job(job->id);
}

void continue_job(job_t *job, int foreground)
{
    if (foreground) {
        printf("%s\n", job->cmdline);
        if (shell_is_interactive)
            tcsetpgrp(shell_terminal, job->pgid);
        job->status = JOB_RUNNING;
        kill(-job->pgid, SIGCONT);
        wait_for_job(job);
        if (shell_is_interactive)
            tcsetpgrp(shell_terminal, shell_pgid);
    } else {
        printf("[%d]+ %s &\n", job->id, job->cmdline);
        job->status = JOB_RUNNING;
        kill(-job->pgid, SIGCONT);
    }
}
