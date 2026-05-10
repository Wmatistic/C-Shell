#include "mysh.h"

static const char *builtins[] = {
    "cd", "exit", "export", "unset",
    "jobs", "fg", "bg", "history", "help", NULL
};

int is_builtin(const char *name)
{
    for (int i = 0; builtins[i]; i++)
        if (strcmp(builtins[i], name) == 0) return 1;
    return 0;
}

static int builtin_cd(cmd_t *cmd)
{
    const char *dir;

    if (cmd->argc < 2) {
        dir = getenv("HOME");
        if (!dir) { fprintf(stderr, "cd: HOME not set\n"); return 1; }
    } else if (strcmp(cmd->argv[1], "-") == 0) {
        dir = getenv("OLDPWD");
        if (!dir) { fprintf(stderr, "cd: OLDPWD not set\n"); return 1; }
        printf("%s\n", dir);
    } else {
        dir = cmd->argv[1];
    }

    char old[4096];
    if (getcwd(old, sizeof(old)) == NULL) old[0] = '\0';

    if (chdir(dir) < 0) { perror(dir); return 1; }

    setenv("OLDPWD", old, 1);
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd))) setenv("PWD", cwd, 1);
    return 0;
}

static int builtin_export(cmd_t *cmd)
{
    if (cmd->argc < 2) {
        extern char **environ;
        for (char **e = environ; *e; e++) printf("export %s\n", *e);
        return 0;
    }
    for (int i = 1; i < cmd->argc; i++) {
        char *eq = strchr(cmd->argv[i], '=');
        if (eq) {
            char name[256];
            int nlen = eq - cmd->argv[i];
            strncpy(name, cmd->argv[i], nlen);
            name[nlen] = '\0';
            setenv(name, eq + 1, 1);
        } else {
            char *val = getenv(cmd->argv[i]);
            setenv(cmd->argv[i], val ? val : "", 1);
        }
    }
    return 0;
}

static int builtin_unset(cmd_t *cmd)
{
    for (int i = 1; i < cmd->argc; i++) unsetenv(cmd->argv[i]);
    return 0;
}

static int builtin_jobs(cmd_t *cmd)
{
    (void)cmd;
    notify_jobs();
    print_jobs();
    return 0;
}

static int builtin_fg(cmd_t *cmd)
{
    int id = cmd->argc >= 2 ? atoi(cmd->argv[1]) : -1;
    job_t *job = id > 0 ? find_job_by_id(id) : job_list;
    if (!job) { fprintf(stderr, "fg: no such job\n"); return 1; }
    continue_job(job, 1);
    return last_exit_status;
}

static int builtin_bg(cmd_t *cmd)
{
    int id = cmd->argc >= 2 ? atoi(cmd->argv[1]) : -1;
    job_t *job = id > 0 ? find_job_by_id(id) : job_list;
    if (!job) { fprintf(stderr, "bg: no such job\n"); return 1; }
    continue_job(job, 0);
    return 0;
}

static int builtin_history(cmd_t *cmd)
{
    int n = cmd->argc >= 2 ? atoi(cmd->argv[1]) : 20;

    HIST_ENTRY **hist = history_list();
    if (!hist) return 0;

    int total = 0;
    while (hist[total]) total++;

    int start = total - n;
    if (start < 0) start = 0;

    for (int i = start; i < total; i++)
        printf("  %4d  %s\n", i + history_base, hist[i]->line);

    return 0;
}

static int builtin_help(cmd_t *cmd)
{
    (void)cmd;
    printf(
        "mysh %s\n"
        "  cd [dir|-]        change directory\n"
        "  export [K=V]      set or print environment variables\n"
        "  unset NAME        unset a variable\n"
        "  jobs              list background/stopped jobs\n"
        "  fg [n]            bring job n to foreground\n"
        "  bg [n]            resume stopped job n in background\n"
        "  history [n]       show last n history entries (default 20)\n"
        "  exit [code]       exit\n"
        "  help              this message\n"
        "\noperators: | && || ; & > >> < 2> &>\n",
        MYSH_VERSION
    );
    return 0;
}

static int builtin_exit(cmd_t *cmd)
{
    int code = cmd->argc >= 2 ? atoi(cmd->argv[1]) : last_exit_status;
    save_history();
    exit(code);
}

int run_builtin(cmd_t *cmd)
{
    const char *name = cmd->argv[0];
    if (strcmp(name, "cd")      == 0) return builtin_cd(cmd);
    if (strcmp(name, "export")  == 0) return builtin_export(cmd);
    if (strcmp(name, "unset")   == 0) return builtin_unset(cmd);
    if (strcmp(name, "jobs")    == 0) return builtin_jobs(cmd);
    if (strcmp(name, "fg")      == 0) return builtin_fg(cmd);
    if (strcmp(name, "bg")      == 0) return builtin_bg(cmd);
    if (strcmp(name, "history") == 0) return builtin_history(cmd);
    if (strcmp(name, "help")    == 0) return builtin_help(cmd);
    if (strcmp(name, "exit")    == 0) return builtin_exit(cmd);
    fprintf(stderr, "mysh: unknown builtin: %s\n", name);
    return 1;
}
