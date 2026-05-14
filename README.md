# unix-shell
A POSIX-compliant shell implemented in C — supports command execution, I/O redirection, piping, and signal handling.

---

## Features

### Command Execution
Forks a child process and uses `execvp` to run any program on the system `PATH`. Correctly captures and tracks the exit status of each command, accessible via `$?`.

### Built-in Commands
| Command | Description |
|--------|-------------|
| `cd [dir]` | Change directory. Defaults to `$HOME` if no argument is given. Updates `$PWD`. |
| `exit` | Exit the shell. |

### I/O Redirection
| Operator | Description |
|----------|-------------|
| `<`  | Redirect stdin from a file |
| `>`  | Redirect stdout to a file (truncate) |
| `>>` | Redirect stdout to a file (append) |
| `2>` | Redirect stderr to a file |
| `&>` | Redirect both stdout and stderr to a file |

### Piping
Supports a single pipe (`|`) between two commands. Each side of the pipe runs in its own child process, with stdout of the left command connected to stdin of the right command via `pipe(2)` and `dup2(2)`.

### Signal Handling
- **SIGINT** (`Ctrl+C`) — Prints a newline and returns to the prompt without exiting the shell.
- **SIGQUIT** (`Ctrl+\`) — Ignored by the shell.
- Child processes have signal dispositions reset to default before `exec`.

### Job Control
- Each foreground command given its own process group with `setpgid(2)`. Both parent and child call `setpgid` to aboid a race condition where child may exec before parent can place the child in its own process group
- The shell transfers terminal control to the foregroud process with `tcsetpgrp(3)` and reclaims control after the child process exits or stops.
- `SIGTTOU` is permanently ignored in the shell process to allow for the shell process to reclaim terminal control

### Dynamic Token Parsing
Input lines are tokenized by whitespace. The token buffer grows dynamically via `realloc` as needed. The special token `$?` is expanded to the last command's exit status at parse time.

### Custom Prompt
Displays `$PS1` as the prompt if set, otherwise falls back to the current working directory (`$PWD`).

---

## Build

```bash
make
```

Requires `gcc` and a POSIX-compliant system (Linux or macOS).

To remove build artifacts:

```bash
make clean
```

---

## Usage

```bash
./shell
```

### Examples

```bash
# Basic command
/home/user> ls -la

# Redirect output to a file
/home/user> ls > output.txt

# Append to a file
/home/user> echo "hello" >> log.txt

# Redirect stderr
/home/user> cat missing.txt 2> err.txt

# Pipe two commands
/home/user> ls | grep ".c"

# Check last exit status
/home/user> echo $?

# Change directory
/home/user> cd /tmp
```

---

## Implementation Notes

- All system calls are checked for errors and propagate failure up the call stack via return values.
- Signal-safe I/O (`write`) is used inside signal handlers rather than `printf`.
- `_exit` is used in child processes after a failed `exec` to avoid flushing shared stdio buffers.
- The shell loops with `waitpid(..., WNOHANG)` retry logic to correctly handle `EINTR` from signal interrupts during `wait`.

---

## Author

Ivan Yeung — NYU Tandon, Unix System Programming
