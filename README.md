# mysh

A POSIX-compliant shell written in C. Supports pipelines, I/O redirection, job control, variable expansion, glob patterns, and command history.

## Features

- **Pipelines** — `cmd1 | cmd2 | cmd3`
- **Command lists** — sequential (`;`), conditional (`&&`, `||`), and background (`&`)
- **I/O redirection** — `>`, `>>`, `<`, `2>`, `&>`
- **Variable expansion** — `$VAR`, `${VAR}`, `$?`, `$$`, `$!`, `~`
- **Glob expansion** — `*`, `?`, `[...]`
- **Job control** — `Ctrl+Z` to stop, `fg`/`bg` to resume, `jobs` to list
- **Command history** — readline editing with persistent history (`~/.mysh_history`)
- **Builtins** — `cd`, `export`, `unset`, `jobs`, `fg`, `bg`, `history`, `help`, `exit`

## Building

**Requires:** `gcc`, `libreadline-dev`

```bash
# Install readline on Debian/Ubuntu
sudo apt install libreadline-dev

make          # optimized build
make debug    # debug build with AddressSanitizer + UBSan
make clean    # remove build artifacts
```

The binary is placed at `./mysh`.

## Usage

```bash
./mysh                  # interactive shell
./mysh --debug          # interactive shell with token/AST debug output
./mysh --version        # print version and exit
./mysh --help           # print usage and exit
```

### Examples

```sh
# Pipelines
echo hello | cat | tr a-z A-Z

# Redirection
echo "log entry" >> log.txt
ls 2> /dev/null
ls -la &> output.txt

# Command lists
make && echo "build ok" || echo "build failed"
cd /tmp; pwd; cd -

# Variable expansion
export NAME=world
echo "Hello, $NAME"    # Hello, world
echo $?                # exit status of last command

# Glob expansion
echo src/*.c

# Background jobs
sleep 10 &
jobs
fg 1
```

## Architecture

```
include/mysh.h      — types, structs, and all function declarations
src/main.c          — entry point, signal setup, terminal setup, REPL
src/lexer.c         — tokenizer (handles quoting, escapes, operators)
src/parser.c        — recursive-descent parser → cmd_t AST with pipelines and lists
src/executor.c      — pipeline execution with POSIX job control
src/jobs.c          — job list, SIGCHLD handler, wait_for_job (sigsuspend-based)
src/expand.c        — variable and glob expansion
src/redirect.c      — apply I/O redirections (dup2)
src/builtins.c      — built-in command implementations
src/history.c       — readline history init/save, xmalloc/xstrdup utilities
```

### Job control design

The shell uses a race-free pattern for foreground jobs:

1. **Block `SIGCHLD`** before `fork()` so no child can be reaped before `add_job()` records it.
2. **Unblock `SIGCHLD`** after `add_job()` — any pending signal fires immediately and updates the job status.
3. **`wait_for_job`** uses `sigsuspend` to atomically sleep and allow `SIGCHLD` delivery, avoiding the classic `SA_RESTART` + blocking-`waitpid` deadlock.

## Testing

```bash
bash run_tests.sh
```

Runs 10 smoke tests covering echo, pipes, exit status, variable expansion, redirections, `cd`/`pwd`, and glob expansion.

## Builtins reference

| Command | Description |
|---------|-------------|
| `cd [dir\|-]` | Change directory; `-` goes to previous directory |
| `export [K=V]` | Set or list environment variables |
| `unset NAME` | Remove an environment variable |
| `jobs` | List background and stopped jobs |
| `fg [n]` | Bring job `n` to the foreground |
| `bg [n]` | Resume stopped job `n` in the background |
| `history [n]` | Show last `n` history entries (default 20) |
| `exit [code]` | Exit with optional status code |
| `help` | Print builtin reference |
