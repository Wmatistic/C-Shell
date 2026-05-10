#include "mysh.h"

char *expand_vars(const char *word)
{
    char       out[MAX_LINE];
    int        oi = 0;
    const char *p = word;

    if (p[0] == '~' && (p[1] == '/' || p[1] == '\0')) {
        const char *home = getenv("HOME");
        if (!home) {
            struct passwd *pw = getpwuid(getuid());
            home = pw ? pw->pw_dir : "";
        }
        int len = strlen(home);
        memcpy(out + oi, home, len);
        oi += len;
        p++;
    }

    while (*p && oi < MAX_LINE - 1) {
        if (*p != '$') { out[oi++] = *p++; continue; }
        p++;

        if (*p == '?') {
            oi += snprintf(out + oi, MAX_LINE - oi, "%d", last_exit_status);
            p++; continue;
        }
        if (*p == '$') {
            oi += snprintf(out + oi, MAX_LINE - oi, "%d", (int)getpid());
            p++; continue;
        }
        if (*p == '!') {
            int bgpid = 0;
            for (job_t *j = job_list; j; j = j->next) bgpid = j->pgid;
            oi += snprintf(out + oi, MAX_LINE - oi, "%d", bgpid);
            p++; continue;
        }

        if (*p == '{') {
            p++;
            char name[256]; int ni = 0;
            while (*p && *p != '}') name[ni++] = *p++;
            name[ni] = '\0';
            if (*p == '}') p++;
            const char *val = getenv(name);
            if (val) { int l = strlen(val); memcpy(out + oi, val, l); oi += l; }
        } else {
            char name[256]; int ni = 0;
            while (*p && (isalnum((unsigned char)*p) || *p == '_'))
                name[ni++] = *p++;
            name[ni] = '\0';
            if (ni) {
                const char *val = getenv(name);
                if (val) { int l = strlen(val); memcpy(out + oi, val, l); oi += l; }
            } else {
                out[oi++] = '$';
            }
        }
    }
    out[oi] = '\0';
    return xstrdup(out);
}

char **expand_globs(char **argv, int argc, int *new_argc)
{
    char **result = xmalloc(sizeof(char *) * MAX_ARGS);
    int    ri     = 0;

    for (int i = 0; i < argc && ri < MAX_ARGS - 1; i++) {
        if (!strpbrk(argv[i], "*?[")) {
            result[ri++] = xstrdup(argv[i]);
            continue;
        }

        glob_t g;
        if (glob(argv[i], GLOB_TILDE | GLOB_NOCHECK, NULL, &g) == 0) {
            for (size_t j = 0; j < g.gl_pathc && ri < MAX_ARGS - 1; j++)
                result[ri++] = xstrdup(g.gl_pathv[j]);
            globfree(&g);
        } else {
            result[ri++] = xstrdup(argv[i]);
        }
    }
    result[ri] = NULL;
    *new_argc  = ri;
    return result;
}
