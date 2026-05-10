#include "mysh.h"

pid_t   shell_pgid;
int     shell_terminal;
int     shell_is_interactive;
job_t  *job_list         = NULL;
int     last_exit_status = 0;

static int debug_mode = 0;

static void setup_signals(void)
{
    signal(SIGINT,  SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
}

static void setup_terminal(void)
{
    shell_terminal       = STDIN_FILENO;
    shell_is_interactive = isatty(shell_terminal);

    if (shell_is_interactive) {
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);

        shell_pgid = getpid();
        if (setpgid(shell_pgid, shell_pgid) < 0) {
            perror("mysh: setpgid");
            exit(EXIT_FAILURE);
        }
        tcsetpgrp(shell_terminal, shell_pgid);
    }
}

static char *build_prompt(void)
{
    static char prompt[512];
    char cwd[256];
    const char *home = getenv("HOME");

    if (getcwd(cwd, sizeof(cwd)) == NULL)
        strcpy(cwd, "?");

    if (home && strncmp(cwd, home, strlen(home)) == 0) {
        char tmp[256];
        snprintf(tmp, sizeof(tmp), "~%s", cwd + strlen(home));
        strcpy(cwd, tmp);
    }

    snprintf(prompt, sizeof(prompt),
             "\001\033[1;35m\002mysh\001\033[0m\002:\001\033[1;34m\002%s\001\033[0m\002$ ",
             cwd);
    return prompt;
}

static void run_repl(void)
{
    if (shell_is_interactive) {
        char *line;
        while (1) {
            notify_jobs();
            line = readline(build_prompt());
            if (!line) { printf("\n"); break; }
            if (*line == '\0') { free(line); continue; }

            add_history(line);

            token_t *tokens = lex(line);
            free(line);
            if (!tokens) continue;

            if (debug_mode) {
                printf("[debug] tokens:\n");
                print_tokens(tokens);
            }

            cmd_t *cmd = parse(tokens);
            if (!cmd) { free_tokens(tokens); continue; }

            if (debug_mode) {
                printf("[debug] AST:\n");
                print_cmd(cmd);
            }

            last_exit_status = execute(cmd);
            free_cmd(cmd);
            free_tokens(tokens);
        }
    } else {
        char buf[MAX_LINE];
        while (fgets(buf, sizeof(buf), stdin)) {
            buf[strcspn(buf, "\n")] = '\0';
            if (*buf == '\0') continue;

            token_t *tokens = lex(buf);
            if (!tokens) continue;

            cmd_t *cmd = parse(tokens);
            if (!cmd) { free_tokens(tokens); continue; }

            last_exit_status = execute(cmd);
            free_cmd(cmd);
            free_tokens(tokens);
        }
    }

    save_history();
}

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0)
            debug_mode = 1;
        else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("mysh %s\n", MYSH_VERSION);
            return 0;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: mysh [--debug] [--version]\n");
            return 0;
        }
    }

    setup_signals();
    setup_terminal();
    init_history();

    if (shell_is_interactive)
        printf("mysh %s  (type 'exit' to quit)\n", MYSH_VERSION);
    run_repl();
    return last_exit_status;
}
