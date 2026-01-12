#include "../include/shell.h"

extern volatile sig_atomic_t foreground_pid;
extern Job jobs[MAX_JOBS];
extern int job_count;
extern char last_foreground_cmd[];

void setup_signals(void) {
    signal(SIGCHLD, sigchld_handler);
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
}

void sigchld_handler(int sig) {
    // Do not reap here; let main loop update_job_status() reap and print status
    // This avoids losing exit information needed for user-facing messages.
}

void sigint_handler(int sig) {
    if (foreground_pid > 0) {
        // Send SIGINT only to the foreground process
        kill(foreground_pid, SIGINT);
    }
}

void sigtstp_handler(int sig) {
    if (foreground_pid > 0) {
    // Send SIGTSTP to the foreground process group per spec; shell continues
    kill(-foreground_pid, SIGTSTP);
    // Emit a minimal marker so tests can see the suspend immediately; use async-signal-safe write
    const char msg[] = "Stopped\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
    }
}

void print_error(char *message) {
    fprintf(stderr, "%s\n", message);
}
