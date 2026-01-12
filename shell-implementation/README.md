# Unix-like Shell Implementation

## Overview

A fully functional Unix-like shell built from scratch in C, implementing core command-line interface features including process execution, pipes, I/O redirection, background jobs, and several built-in commands. This project demonstrates deep understanding of system programming, process management, and Unix internals.

## Core Features

### 1. Command Execution
- Execute external programs with arguments
- Path resolution for executables
- Error handling for invalid commands
- Exit status tracking

### 2. I/O Redirection
- **Input Redirection (`<`)**: Read from file instead of stdin
- **Output Redirection (`>`)**: Write to file instead of stdout
- **Append Redirection (`>>`)**: Append to file
- Error redirection support

### 3. Pipes (`|`)
- Chain multiple commands together
- Output of one command becomes input to next
- Support for multiple pipes in single command line
- Proper file descriptor management

### 4. Background Execution (`&`)
- Run processes in background
- Non-blocking execution
- Job tracking and status monitoring
- Background job completion notifications

### 5. Job Control
- Track all background processes
- List jobs with status (running/stopped/completed)
- Bring jobs to foreground (`fg`)
- Move jobs to background (`bg`)
- Job numbering and reference

### 6. Signal Handling
- **Ctrl+C (SIGINT)**: Interrupt foreground process (not shell)
- **Ctrl+Z (SIGTSTP)**: Suspend foreground process
- Shell continues running after signals
- Proper signal propagation to child processes

## Built-in Commands

### `hop` - Change Directory
Enhanced version of `cd` with additional features:

```bash
hop                  # Go to home directory
hop ~                # Go to home directory
hop path/to/dir      # Change to specified directory
hop -                # Return to previous directory
hop ..               # Go to parent directory
```

**Features:**
- Maintains previous directory for quick switching
- Resolves `~` to home directory
- Updates shell prompt with new directory
- Error handling for invalid paths

### `reveal` - List Directory Contents
Custom implementation of `ls` with detailed output:

```bash
reveal               # List current directory
reveal path          # List specified directory
reveal -a            # Show hidden files
reveal -l            # Long format with details
reveal -al path      # Combine flags
```

**Long Format Displays:**
- File permissions (rwxrwxrwx)
- Number of hard links
- Owner name
- Group name
- File size in bytes
- Last modification date and time
- Filename (color-coded by type)

**Features:**
- Hidden file support (files starting with `.`)
- Directory vs file indication
- Sorted output
- Color-coded output (directories, executables, files)

### `log` - Command History
Persistent command history with execution support:

```bash
log                  # Display last 15 commands
log execute 5        # Re-execute 5th command from log
log purge            # Clear all history
```

**Features:**
- Stores up to 15 most recent commands
- Persists across shell sessions
- Saves to file in home directory (`~/.shell_log`)
- Prevents duplicate consecutive commands
- Circular buffer implementation

### `proclore` - Process Information
Display detailed information about processes:

```bash
proclore             # Info about shell itself
proclore 1234        # Info about process 1234
```

**Displays:**
- Process ID (PID)
- Process status (R/S/Z/T)
- Process group
- Virtual memory size
- Executable path

### `seek` - Search for Files/Directories
Search for files and directories by name:

```bash
seek filename                    # Search in current directory
seek -d dirname                  # Search only directories
seek -f filename                 # Search only files
seek -e filename target_dir      # Search and execute/display
```

**Features:**
- Recursive search through directory tree
- Filter by file type (file/directory)
- Execute if single match and `-e` flag
- Color-coded results

### `activities` - Background Job List
Display all background processes:

```bash
activities
```

**Output Format:**
```
[1] running : command arg1 arg2 : 12345
[2] stopped : another_command : 12346
```

### `ping` - Send Signals to Jobs
Send signals to background jobs:

```bash
ping <job_number> <signal_number>
ping 1 9              # Send SIGKILL to job 1
ping 2 19             # Send SIGSTOP to job 2
```

### `fg` & `bg` - Foreground/Background Control
```bash
fg <job_number>       # Bring job to foreground
bg <job_number>       # Resume job in background
```

## Technical Implementation

### Architecture

```
┌─────────────────────────────────────────┐
│           Main Shell Loop               │
│  1. Display prompt                      │
│  2. Read user input                     │
│  3. Parse command line                  │
│  4. Execute commands                    │
│  5. Check background jobs               │
└─────────────────────────────────────────┘
         │
         ├─→ [Parser] ─→ Tokenize, identify pipes, redirection
         │
         ├─→ [Executor] ─→ Fork, exec, wait
         │
         ├─→ [Job Manager] ─→ Track background processes
         │
         └─→ [Signal Handler] ─→ Handle Ctrl+C, Ctrl+Z
```

### Key Files

```
shell/
├── Makefile                # Build configuration
├── include/
│   └── shell.h            # Header with all declarations
└── src/
    ├── main.c             # Main loop and initialization
    ├── parser.c           # Command parsing and tokenization
    ├── execute.c          # Command execution logic
    ├── hop.c              # cd implementation
    ├── jobs.c             # Job control and management
    ├── log.c              # History management
    └── signals.c          # Signal handling
```

### Process Management

#### Command Execution Flow
```c
// Simplified execution logic
void execute_command(char *cmd) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process
        setup_redirections();
        setup_pipes();
        execvp(program, args);
        // If exec fails
        perror("exec failed");
        exit(1);
    } else {
        // Parent process
        if (background) {
            add_to_job_list(pid, cmd);
            printf("[%d] %d\n", job_id, pid);
        } else {
            waitpid(pid, &status, WUNTRACED);
        }
    }
}
```

#### Pipe Implementation
```c
void execute_pipeline(char **commands, int num_cmds) {
    int pipes[num_cmds-1][2];
    
    // Create all pipes
    for (int i = 0; i < num_cmds-1; i++) {
        pipe(pipes[i]);
    }
    
    // Fork and setup each command
    for (int i = 0; i < num_cmds; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Setup input from previous pipe
            if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            // Setup output to next pipe
            if (i < num_cmds-1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            // Close all pipe fds
            close_all_pipes(pipes, num_cmds-1);
            // Execute command
            execvp(program, args);
        }
    }
    
    // Parent closes all pipes and waits
    close_all_pipes(pipes, num_cmds-1);
    wait_for_children(num_cmds);
}
```

### Signal Handling

```c
void setup_signal_handlers() {
    signal(SIGINT, sigint_handler);   // Ctrl+C
    signal(SIGTSTP, sigtstp_handler); // Ctrl+Z
    signal(SIGCHLD, sigchld_handler); // Child termination
}

void sigint_handler(int signum) {
    // Don't terminate shell, only foreground process
    if (foreground_pid > 0) {
        kill(foreground_pid, SIGINT);
    }
    printf("\n");
    display_prompt();
}
```

### File Descriptor Management

```c
// Example: Output redirection
void redirect_output(char *filename) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open failed");
        return;
    }
    dup2(fd, STDOUT_FILENO);  // Replace stdout with file
    close(fd);                 // Close original fd
}
```

## Building and Running

### Compilation

```bash
make
```

Or manually:
```bash
gcc -o shell src/*.c -I include -Wall -Wextra
```

### Execution

```bash
./shell
```

### Example Session

```bash
user@hostname:~/project$ ls -l
total 24
drwxr-xr-x 2 user user 4096 Jan 17 10:30 src
-rw-r--r-- 1 user user  512 Jan 17 10:25 Makefile

user@hostname:~/project$ cat file.txt | grep "pattern" | wc -l
42

user@hostname:~/project$ sleep 100 &
[1] 12345

user@hostname:~/project$ activities
[1] running : sleep 100 : 12345

user@hostname:~/project$ hop -
user@hostname:~/previous$ 

user@hostname:~/previous$ log
1: ls -l
2: cat file.txt | grep "pattern" | wc -l
3: sleep 100 &
4: activities
5: hop -

user@hostname:~/previous$ log execute 2
(re-executes: cat file.txt | grep "pattern" | wc -l)
```

## Key Concepts Demonstrated

1. **Process Creation and Management**
   - `fork()` for process creation
   - `exec()` family for program loading
   - `wait()` and `waitpid()` for synchronization

2. **File Descriptor Manipulation**
   - `dup2()` for redirection
   - `pipe()` for inter-process communication
   - Proper descriptor closing to prevent leaks

3. **Process Groups and Sessions**
   - `setpgid()` for job control
   - Process group management
   - Terminal control

4. **Signal Handling**
   - `signal()` and `sigaction()` registration
   - Safe signal handler implementation
   - Signal propagation

5. **File I/O**
   - Opening, reading, writing files
   - File permissions and modes
   - Error handling

6. **Memory Management**
   - Dynamic allocation with `malloc()`
   - Proper `free()` to prevent leaks
   - String manipulation

7. **Parsing and Tokenization**
   - Command line parsing
   - Special character handling
   - Quote and escape processing

## Testing

Test with various scenarios:
- Simple commands: `ls`, `pwd`, `echo hello`
- Pipes: `cat file | grep pattern | sort`
- Redirection: `ls > output.txt`, `cat < input.txt`
- Background: `sleep 30 &`, `find / -name file &`
- Job control: `Ctrl+Z`, `fg`, `bg`
- Built-ins: `hop`, `reveal -al`, `log`
- Error cases: invalid commands, permission errors

## Limitations

- No command-line editing (arrow keys for history)
- No tab completion
- Limited wildcard expansion
- No command substitution (backticks or `$()`)
- No environment variable expansion beyond `$HOME`
- No scripting support (if/then/else, loops)

## Learning Outcomes

- Deep understanding of Unix process model
- System call interfaces and semantics
- Inter-process communication mechanisms
- File descriptor and I/O handling
- Signal handling and process control
- Parser design and implementation
- Resource management in C
- Error handling best practices

## References

- "Advanced Programming in the UNIX Environment" by Stevens & Rago
- "The Linux Programming Interface" by Michael Kerrisk
- bash source code
- POSIX shell specification

---

This shell implementation represents a substantial systems programming project, demonstrating proficiency with low-level Unix APIs and understanding of operating system concepts.
