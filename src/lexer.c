#include "mysh.h"

static token_t *make_token(tok_type_t type, const char *val)
{
    token_t *t = xmalloc(sizeof(token_t));
    t->type    = type;
    t->val     = val ? xstrdup(val) : NULL;
    t->next    = NULL;
    return t;
}

static char *read_word(const char **p)
{
    char buf[MAX_LINE];
    int  i = 0;

    while (**p && **p != ' ' && **p != '\t' && **p != '\n'
           && **p != '|' && **p != '<' && **p != '>'
           && **p != '&' && **p != ';') {

        if (**p == '\\' && *(*p + 1)) {
            (*p)++;
            buf[i++] = **p;
            (*p)++;
            continue;
        }

        if (**p == '\'') {
            (*p)++;
            while (**p && **p != '\'')
                buf[i++] = *(*p)++;
            if (**p == '\'') (*p)++;
            continue;
        }

        if (**p == '"') {
            (*p)++;
            while (**p && **p != '"') {
                if (**p == '\\' && *(*p + 1)) (*p)++;
                buf[i++] = *(*p)++;
            }
            if (**p == '"') (*p)++;
            continue;
        }

        buf[i++] = *(*p)++;
    }
    buf[i] = '\0';
    return xstrdup(buf);
}

token_t *lex(const char *line)
{
    token_t    *head = NULL, *tail = NULL;
    const char *p    = line;

/* evaluate tok exactly once to avoid double-call side effects */
#define APPEND(tok) do {                              \
    token_t *_tok_ = (tok);                           \
    if (!head) head = tail = _tok_;                   \
    else { tail->next = _tok_; tail = _tok_; }        \
} while (0)

    while (*p) {
        if (*p == ' ' || *p == '\t' || *p == '\n') { p++; continue; }
        if (*p == '#') break;
        if (*p == ';')  { APPEND(make_token(TOK_SEMI, NULL)); p++; continue; }

        if (*p == '&') {
            if (*(p+1) == '&')      { APPEND(make_token(TOK_AND,        "&&")); p += 2; }
            else if (*(p+1) == '>') { APPEND(make_token(TOK_REDIR_BOTH, NULL)); p += 2; }
            else                    { APPEND(make_token(TOK_AMP,         NULL)); p++;    }
            continue;
        }

        if (*p == '|') {
            if (*(p+1) == '|') { APPEND(make_token(TOK_OR,   "||")); p += 2; }
            else               { APPEND(make_token(TOK_PIPE,  NULL)); p++;    }
            continue;
        }

        if (*p == '>' && *(p+1) == '>') { APPEND(make_token(TOK_REDIR_APP, NULL)); p += 2; continue; }
        if (*p == '2' && *(p+1) == '>') { APPEND(make_token(TOK_REDIR_ERR, NULL)); p += 2; continue; }
        if (*p == '>')                   { APPEND(make_token(TOK_REDIR_OUT, NULL)); p++;    continue; }
        if (*p == '<')                   { APPEND(make_token(TOK_REDIR_IN,  NULL)); p++;    continue; }

        char *word = read_word(&p);
        APPEND(make_token(TOK_WORD, word));
        free(word);
    }

    APPEND(make_token(TOK_EOF, NULL));
    return head;

#undef APPEND
}

void free_tokens(token_t *head)
{
    while (head) {
        token_t *next = head->next;
        free(head->val);
        free(head);
        head = next;
    }
}

static const char *tok_name(tok_type_t t)
{
    switch (t) {
        case TOK_WORD:       return "WORD";
        case TOK_PIPE:       return "PIPE";
        case TOK_AND:        return "AND";
        case TOK_OR:         return "OR";
        case TOK_SEMI:       return "SEMI";
        case TOK_AMP:        return "AMP";
        case TOK_REDIR_IN:   return "REDIR_IN";
        case TOK_REDIR_OUT:  return "REDIR_OUT";
        case TOK_REDIR_APP:  return "REDIR_APP";
        case TOK_REDIR_ERR:  return "REDIR_ERR";
        case TOK_REDIR_BOTH: return "REDIR_BOTH";
        case TOK_EOF:        return "EOF";
        default:             return "?";
    }
}

void print_tokens(token_t *head)
{
    for (token_t *t = head; t; t = t->next)
        printf("  [%-12s] %s\n", tok_name(t->type), t->val ? t->val : "");
}
