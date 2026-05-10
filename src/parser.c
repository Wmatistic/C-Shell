#include "mysh.h"

static token_t *cur;

static void advance(void)
{
    if (cur && cur->type != TOK_EOF)
        cur = cur->next;
}

static int at(tok_type_t t)
{
    return cur && cur->type == t;
}

static int is_pipeline_end(void)
{
    return !cur || at(TOK_EOF) || at(TOK_AMP)
        || at(TOK_SEMI) || at(TOK_AND) || at(TOK_OR) || at(TOK_PIPE);
}

static cmd_t *parse_simple_cmd(void)
{
    cmd_t *cmd     = xmalloc(sizeof(cmd_t));
    cmd->argv      = xmalloc(sizeof(char *) * MAX_ARGS);
    cmd->argc      = 0;
    cmd->redirs    = NULL;
    cmd->pipe_next = NULL;
    cmd->list_next = NULL;
    cmd->list_op   = LIST_NONE;
    cmd->background = 0;

    redir_t *redir_tail = NULL;

    while (!is_pipeline_end()) {
        if (at(TOK_REDIR_IN)  || at(TOK_REDIR_OUT) || at(TOK_REDIR_APP)
         || at(TOK_REDIR_ERR) || at(TOK_REDIR_BOTH)) {
            redir_t *r = xmalloc(sizeof(redir_t));
            r->type = cur->type;
            r->file = NULL;
            r->next = NULL;
            advance();

            if (!at(TOK_WORD)) {
                fprintf(stderr, "mysh: expected filename after redirection\n");
                free(r);
                continue;
            }
            r->file = xstrdup(cur->val);
            advance();

            if (!cmd->redirs) cmd->redirs = redir_tail = r;
            else { redir_tail->next = r; redir_tail = r; }
            continue;
        }

        if (at(TOK_WORD)) {
            if (cmd->argc < MAX_ARGS - 1)
                cmd->argv[cmd->argc++] = xstrdup(cur->val);
            advance();
            continue;
        }

        advance();
    }

    cmd->argv[cmd->argc] = NULL;

    if (cmd->argc == 0 && !cmd->redirs) {
        free_cmd(cmd);
        return NULL;
    }
    return cmd;
}

static cmd_t *parse_pipeline(void)
{
    cmd_t *head = parse_simple_cmd();
    if (!head) return NULL;

    cmd_t *tail = head;

    while (at(TOK_PIPE)) {
        advance();
        cmd_t *next = parse_simple_cmd();
        if (!next) break;
        tail->pipe_next = next;
        tail = next;
    }

    if (at(TOK_AMP)) {
        head->background = 1;
        advance();
    }

    return head;
}

cmd_t *parse(token_t *tokens)
{
    cur = tokens;
    if (!cur || at(TOK_EOF)) return NULL;

    cmd_t *first = parse_pipeline();
    if (!first) return NULL;

    cmd_t *tail = first;
    while (at(TOK_SEMI) || at(TOK_AND) || at(TOK_OR)) {
        list_op_t op;
        if      (at(TOK_AND))  op = LIST_AND;
        else if (at(TOK_OR))   op = LIST_OR;
        else                   op = LIST_SEMI;
        advance();

        if (!cur || at(TOK_EOF)) break;

        cmd_t *next = parse_pipeline();
        if (!next) break;

        tail->list_op   = op;
        tail->list_next = next;
        tail = next;
    }

    return first;
}

void free_cmd(cmd_t *cmd)
{
    if (!cmd) return;
    free_cmd(cmd->list_next);
    free_cmd(cmd->pipe_next);
    for (int i = 0; i < cmd->argc; i++) free(cmd->argv[i]);
    free(cmd->argv);
    redir_t *r = cmd->redirs;
    while (r) { redir_t *n = r->next; free(r->file); free(r); r = n; }
    free(cmd);
}

void print_cmd(cmd_t *cmd)
{
    if (!cmd) return;
    int stage = 0;
    for (cmd_t *c = cmd; c; c = c->pipe_next, stage++) {
        printf("  [stage %d]", stage);
        for (int i = 0; i < c->argc; i++) printf(" %s", c->argv[i]);
        if (c->background) printf("  &");
        for (redir_t *r = c->redirs; r; r = r->next)
            printf("  redir(%d->%s)", r->type, r->file);
        printf("\n");
    }
    if (cmd->list_next) {
        const char *op = cmd->list_op == LIST_AND ? "&&"
                       : cmd->list_op == LIST_OR  ? "||" : ";";
        printf("  [%s]\n", op);
        print_cmd(cmd->list_next);
    }
}
