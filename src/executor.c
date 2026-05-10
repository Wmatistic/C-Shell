#include "mysh.h"

static int pipeline_length(cmd_t *cmd)
{
    int n = 0;
    for (cmd_t *c = cmd; c; c = c->pipe_next) n++;
    return n;
}

static pid_t launch_process(cmd_t *cmd, pid_t pgid,
                             int in_fd, int out_fd, int foreground)
{
    char **expanded = xmalloc(sizeof(char *) * MAX_ARGS);
    int    exp_argc = 0;

    for (int i = 0; i < cmd->argc && exp_argc < MAX_ARGS - 1; i++)
        expanded[exp_argc++] = expand_vars(cmd->argv[i]);
    expanded[exp_argc] = NULL;

    int    glob_argc  = 0;
    char **final_argv = expand_globs(expanded, exp_argc, &glob_argc);
    for (int i = 0; i < exp_argc; i++) free(expanded[i]);
    free(expanded);

    pid_t pid = fork();
    if (pid < 0) { perror("mysh: fork"); return -1; }

    if (pid == 0) {
        pid_t mypid = getpid();
        if (pgid == 0) pgid = mypid;
        setpgid(mypid, pgid);

        if (foreground && shell_is_interactive)
            tcsetpgrp(shell_terminal, pgid);

        signal(SIGINT,  SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        /* unblock all signals that the shell may have blocked */
        sigset_t all;
        sigfillset(&all);
        sigprocmask(SIG_UNBLOCK, &all, NULL);

        if (in_fd != STDIN_FILENO)  { dup2(in_fd,  STDIN_FILENO);  close(in_fd);  }
        if (out_fd != STDOUT_FILENO){ dup2(out_fd, STDOUT_FILENO); close(out_fd); }

        if (apply_redirs(cmd->redirs) < 0)
            exit(EXIT_FAILURE);

        execvp(final_argv[0], final_argv);
        fprintf(stderr, "mysh: %s: %s\n", final_argv[0], strerror(errno));
        exit(127);
    }

    for (int i = 0; i < glob_argc; i++) free(final_argv[i]);
    free(final_argv);

    if (pgid == 0) pgid = pid;
    setpgid(pid, pgid);

    return pid;
}

int execute_pipeline(cmd_t *head, int background)
{
    int   n     = pipeline_length(head);
    pid_t pgid  = 0;
    int   in_fd = STDIN_FILENO;
    int   pfd[2];

    pid_t *pids   = xmalloc(sizeof(pid_t) * n);
    cmd_t *cmd    = head;
    char  *cmdline = head->argc ? head->argv[0] : "?";

    /* Block SIGCHLD so no child can be reaped before add_job() records it */
    sigset_t chld, prev;
    sigemptyset(&chld);
    sigaddset(&chld, SIGCHLD);
    sigprocmask(SIG_BLOCK, &chld, &prev);

    for (int i = 0; i < n; i++, cmd = cmd->pipe_next) {
        int out_fd = STDOUT_FILENO;

        if (i < n - 1) {
            if (pipe(pfd) < 0) { perror("mysh: pipe"); free(pids); return 1; }
            out_fd = pfd[1];
        }

        pids[i] = launch_process(cmd, pgid, in_fd, out_fd, !background);
        if (pids[i] < 0) { free(pids); return 1; }
        if (pgid == 0) pgid = pids[i];

        if (in_fd  != STDIN_FILENO)  close(in_fd);
        if (out_fd != STDOUT_FILENO) close(out_fd);

        in_fd = (i < n - 1) ? pfd[0] : STDIN_FILENO;
    }

    job_t *job = add_job(pgid, cmdline, JOB_RUNNING);

    /* Unblock SIGCHLD — any pending signal fires now that the job is recorded */
    sigprocmask(SIG_SETMASK, &prev, NULL);

    if (background) {
        printf("[%d] %d\n", job->id, pgid);
        free(pids);
        return 0;
    }

    if (shell_is_interactive)
        tcsetpgrp(shell_terminal, pgid);

    wait_for_job(job);

    if (shell_is_interactive)
        tcsetpgrp(shell_terminal, shell_pgid);

    int status = last_exit_status;
    free(pids);
    return status;
}

static int execute_one(cmd_t *cmd)
{
    if (!cmd || cmd->argc == 0) return 0;

    if (!cmd->pipe_next && is_builtin(cmd->argv[0]))
        return run_builtin(cmd);

    return execute_pipeline(cmd, cmd->background);
}

int execute(cmd_t *cmd)
{
    int status = 0;
    for (cmd_t *c = cmd; c; ) {
        status = execute_one(c);
        last_exit_status = status;

        /* c->list_op is the operator connecting c to c->list_next */
        cmd_t *next = c->list_next;
        if (next) {
            if (c->list_op == LIST_AND && status != 0) break;
            if (c->list_op == LIST_OR  && status == 0) break;
        }
        c = next;
    }
    return status;
}
