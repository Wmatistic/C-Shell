#ifndef MYSH_H
#define MYSH_H

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <glob.h>
#include <pwd.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MYSH_VERSION "0.2.0"
#define MAX_ARGS     256
#define MAX_LINE     4096

/* ── tokens ──────────────────────────────────────────────────────────── */
typedef enum {
    TOK_WORD,
    TOK_PIPE,       /* |  */
    TOK_AND,        /* && */
    TOK_OR,         /* || */
    TOK_SEMI,       /* ;  */
    TOK_AMP,        /* &  */
    TOK_REDIR_IN,   /* <  */
    TOK_REDIR_OUT,  /* >  */
    TOK_REDIR_APP,  /* >> */
    TOK_REDIR_ERR,  /* 2> */
    TOK_REDIR_BOTH, /* &> */
    TOK_EOF
} tok_type_t;

typedef struct token {
    tok_type_t    type;
    char         *val;
    struct token *next;
} token_t;

/* ── AST ─────────────────────────────────────────────────────────────── */
typedef struct redir {
    tok_type_t    type;
    char         *file;
    struct redir *next;
} redir_t;

typedef enum { LIST_NONE = 0, LIST_SEMI, LIST_AND, LIST_OR } list_op_t;

typedef struct cmd {
    char       **argv;
    int          argc;
    redir_t     *redirs;
    struct cmd  *pipe_next;   /* next stage in pipeline */
    struct cmd  *list_next;   /* next command in list   */
    list_op_t    list_op;     /* how list_next is joined */
    int          background;
} cmd_t;

/* ── jobs ────────────────────────────────────────────────────────────── */
typedef enum { JOB_RUNNING, JOB_STOPPED, JOB_DONE } job_status_t;

typedef struct job {
    int           id;
    pid_t         pgid;
    char         *cmdline;
    job_status_t  status;
    int           notified;
    struct job   *next;
} job_t;

/* ── globals ─────────────────────────────────────────────────────────── */
extern pid_t  shell_pgid;
extern int    shell_terminal;
extern int    shell_is_interactive;
extern job_t *job_list;
extern int    last_exit_status;

/* ── lexer.c ─────────────────────────────────────────────────────────── */
token_t *lex(const char *line);
void     free_tokens(token_t *t);

/* ── parser.c ────────────────────────────────────────────────────────── */
cmd_t *parse(token_t *tokens);
void   free_cmd(cmd_t *cmd);

/* ── expand.c ────────────────────────────────────────────────────────── */
char  *expand_vars(const char *s);
char **expand_globs(char **argv, int argc, int *out_argc);

/* ── redirect.c ──────────────────────────────────────────────────────── */
int apply_redirs(redir_t *r);

/* ── executor.c ──────────────────────────────────────────────────────── */
int execute(cmd_t *cmd);

/* ── builtins.c ──────────────────────────────────────────────────────── */
int is_builtin(const char *name);
int run_builtin(cmd_t *cmd);

/* ── jobs.c ──────────────────────────────────────────────────────────── */
job_t *add_job(pid_t pgid, const char *cmdline, job_status_t status);
void   remove_job(int id);
job_t *find_job_by_id(int id);
job_t *find_job_by_pgid(pid_t pgid);
void   print_jobs(void);
void   notify_jobs(void);
void   wait_for_job(job_t *job);
void   continue_job(job_t *job, int foreground);
void   sigchld_handler(int sig);

/* ── utils (history.c) ───────────────────────────────────────────────── */
void  *xmalloc(size_t n);
char  *xstrdup(const char *s);
void   init_history(void);
void   save_history(void);

/* ── debug helpers ───────────────────────────────────────────────────── */
void   print_tokens(token_t *t);
void   print_cmd(cmd_t *c);

#endif
