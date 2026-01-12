/* ############## LLM Generated Code Begins ############## */
#include "../include/shell.h"

extern Job jobs[MAX_JOBS];
extern int job_count;
extern int next_job_id;
extern volatile sig_atomic_t foreground_pid;

void add_job(pid_t pid, char *command, int is_background) {
    if (job_count < MAX_JOBS) {
        jobs[job_count].job_id = next_job_id++;
        jobs[job_count].pid = pid;
        jobs[job_count].command = strdup(command);
        jobs[job_count].is_background = is_background;
        jobs[job_count].is_stopped = 0;
        jobs[job_count].start_time = time(NULL);
        job_count++;
    }
}

void remove_job(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            free(jobs[i].command);
            // Shift remaining jobs
            for (int j = i; j < job_count - 1; j++) {
                jobs[j] = jobs[j + 1];
            }
            job_count--;
            break;
        }
    }
    
    // Reset job ID counter if no jobs left
    if (job_count == 0) {
        next_job_id = 1;
    }
}

void update_job_status(void) {
    for (int i = 0; i < job_count; i++) {
        int status;
        pid_t result = waitpid(jobs[i].pid, &status, WNOHANG);
        
        if (result > 0) {
            // Process has changed state
            print_job_status(jobs[i].pid, status);
            remove_job(jobs[i].pid);
            i--; // Adjust index after removal
        }
    }
}

void print_job_status(pid_t pid, int status) {
    Job *job = find_job_by_pid(pid);
    if (!job) return;
    
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == 0) {
            fprintf(stderr, "%s with pid %d exited normally\n", job->command, pid);
        } else {
            fprintf(stderr, "%s with pid %d exited abnormally\n", job->command, pid);
        }
    } else if (WIFSIGNALED(status)) {
        fprintf(stderr, "%s with pid %d exited abnormally\n", job->command, pid);
    }
}

Job* find_job_by_id(int job_id) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id) {
            return &jobs[i];
        }
    }
    return NULL;
}

Job* find_job_by_pid(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            return &jobs[i];
        }
    }
    return NULL;
}

int activities_command(char **args, int arg_count) {
    if (job_count == 0) {
        return 0; // Print nothing if no jobs
    }
    
    // Sort jobs lexicographically by command name
    Job temp_jobs[MAX_JOBS];
    memcpy(temp_jobs, jobs, job_count * sizeof(Job));
    
    for (int i = 0; i < job_count - 1; i++) {
        for (int j = i + 1; j < job_count; j++) {
            if (strcmp(temp_jobs[i].command, temp_jobs[j].command) > 0) {
                Job temp = temp_jobs[i];
                temp_jobs[i] = temp_jobs[j];
                temp_jobs[j] = temp;
            }
        }
    }
    
    // Print job information
    for (int i = 0; i < job_count; i++) {
        printf("[%d] : %s - %s\n", 
               temp_jobs[i].pid,
               temp_jobs[i].command,
               temp_jobs[i].is_stopped ? "Stopped" : "Running");
    }
    
    return 0;
}

int fg_command(char **args, int arg_count) {
    Job *job = NULL;
    
    if (arg_count == 1) {
        // No job number provided - use most recent background/stopped job
        if (job_count == 0) {
            fprintf(stderr, "No such job\n");
            return 1;
        }
        
        // Find most recently created job (highest job_id)
        int max_job_id = 0;
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].job_id > max_job_id) {
                max_job_id = jobs[i].job_id;
                job = &jobs[i];
            }
        }
    } else if (arg_count == 2) {
        // Job number provided
        int job_id = atoi(args[1]);
        job = find_job_by_id(job_id);
    } else {
        fprintf(stderr, "fg: Invalid Syntax!\n");
        return 1;
    }
    
    if (!job) {
        fprintf(stderr, "No such job\n");
        return 1;
    }
    
    // Print the command being brought to foreground
    printf("%s\n", job->command);
    
    // Bring job to foreground
    foreground_pid = job->pid;
    
    // If stopped, send SIGCONT
    if (job->is_stopped) {
        kill(job->pid, SIGCONT);
        job->is_stopped = 0;
    }
    
    // Wait for the job
    int status;
    waitpid(job->pid, &status, 0);
    foreground_pid = 0;
    
    remove_job(job->pid);
    return 0;
}

int bg_command(char **args, int arg_count) {
    Job *job = NULL;
    
    if (arg_count == 1) {
        // No job number provided - use most recent background/stopped job
        if (job_count == 0) {
            fprintf(stderr, "No such job\n");
            return 1;
        }
        
        // Find most recently created job (highest job_id)
        int max_job_id = 0;
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].job_id > max_job_id) {
                max_job_id = jobs[i].job_id;
                job = &jobs[i];
            }
        }
    } else if (arg_count == 2) {
        // Job number provided
        int job_id = atoi(args[1]);
        job = find_job_by_id(job_id);
    } else {
        fprintf(stderr, "bg: Invalid Syntax!\n");
        return 1;
    }
    
    if (!job) {
        fprintf(stderr, "No such job\n");
        return 1;
    }
    
    // Check if job is already running
    if (!job->is_stopped) {
        fprintf(stderr, "Job already running\n");
        return 1;
    }
    
    // Send SIGCONT to resume stopped job
    if (kill(job->pid, SIGCONT) == 0) {
        job->is_stopped = 0;
        job->is_background = 1;
        printf("[%d] %s &\n", job->job_id, job->command);
    } else {
        perror("bg");
        return 1;
    }
    
    return 0;
}

int ping_command(char **args, int arg_count) {
    if (arg_count != 3) {
        fprintf(stderr, "Invalid syntax!\n");
        return 1;
    }
    
    int pid = atoi(args[1]);
    int signal_num = atoi(args[2]);
    
    // Validate signal number (original must be valid)
    if (signal_num < 1 || signal_num > 31) {
        fprintf(stderr, "Invalid syntax!\n");
        return 1;
    }
    
    // Take signal number modulo 32
    int actual_signal = signal_num % 32;
    
    Job *job = find_job_by_pid(pid);
    if (!job) {
        fprintf(stderr, "No such process found\n");
        return 1;
    }
    
    // Send signal
    if (kill(pid, actual_signal) == -1) {
        perror("ping");
        return 1;
    }
    
    printf("Sent signal %d to process with pid %d\n", signal_num, pid);
    return 0;
}
/* ############## LLM Generated Code Ends ################ */
