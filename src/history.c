#include "mysh.h"

#define HISTORY_FILE ".mysh_history"

static char history_path[4096];

void init_history(void)
{
    const char *home = getenv("HOME");
    if (home)
        snprintf(history_path, sizeof(history_path), "%s/%s", home, HISTORY_FILE);
    else
        snprintf(history_path, sizeof(history_path), "./%s", HISTORY_FILE);
    using_history();
    read_history(history_path);
    stifle_history(1000);
}

void save_history(void)
{
    write_history(history_path);
}

void *xmalloc(size_t n)
{
    void *p = calloc(1, n);
    if (!p) { perror("mysh: malloc"); exit(EXIT_FAILURE); }
    return p;
}

char *xstrdup(const char *s)
{
    char *p = strdup(s);
    if (!p) { perror("mysh: strdup"); exit(EXIT_FAILURE); }
    return p;
}
