#include "mysh.h"

int apply_redirs(redir_t *redirs)
{
    for (redir_t *r = redirs; r; r = r->next) {
        int fd = -1;

        switch (r->type) {
            case TOK_REDIR_IN:
                fd = open(r->file, O_RDONLY);
                if (fd < 0) { perror(r->file); return -1; }
                if (dup2(fd, STDIN_FILENO) < 0)  { perror("dup2"); close(fd); return -1; }
                close(fd);
                break;

            case TOK_REDIR_OUT:
                fd = open(r->file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror(r->file); return -1; }
                if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2"); close(fd); return -1; }
                close(fd);
                break;

            case TOK_REDIR_APP:
                fd = open(r->file, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd < 0) { perror(r->file); return -1; }
                if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2"); close(fd); return -1; }
                close(fd);
                break;

            case TOK_REDIR_ERR:
                fd = open(r->file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror(r->file); return -1; }
                if (dup2(fd, STDERR_FILENO) < 0) { perror("dup2"); close(fd); return -1; }
                close(fd);
                break;

            case TOK_REDIR_BOTH:
                fd = open(r->file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror(r->file); return -1; }
                if (dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0) {
                    perror("dup2"); close(fd); return -1;
                }
                close(fd);
                break;

            default:
                fprintf(stderr, "mysh: unhandled redirection type %d\n", r->type);
                return -1;
        }
    }
    return 0;
}
