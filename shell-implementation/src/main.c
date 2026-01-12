/* ############## LLM Generated Code Begins ############## */
#include "../include/shell.h"

// Global variables
char home_dir[MAX_PATH_SIZE];
char current_dir[MAX_PATH_SIZE];
char previous_dir[MAX_PATH_SIZE];
char shell_name[MAX_HOSTNAME];
char user_name[MAX_HOSTNAME];
// Track the last foreground command string (for job control messages)
char last_foreground_cmd[MAX_INPUT_SIZE];

// Job control variables
Job jobs[MAX_JOBS];
int job_count = 0;
int next_job_id = 1;

// Log variables
char *command_log[MAX_LOG_COMMANDS];
int log_count = 0;
int log_start = 0;

// Signal handling
volatile sig_atomic_t foreground_pid = 0;
int suppress_bg_notice = 0;

int main() {
    init_shell();
    setup_signals();
    init_log();
    
    char *input;
    Pipeline *pipelines;
    int pipeline_count;
    
    while (1) {
        display_prompt();
        
    input = get_input();
        if (!input) {
            // Ctrl+D pressed - EOF
            printf("\nlogout\n");
            // Send SIGKILL to all child processes
            for (int i = 0; i < job_count; i++) {
                kill(jobs[i].pid, SIGKILL);
            }
            exit(0);
        }
        
        // Skip empty input
        char *trimmed_input = trim_whitespace(input);
        if (strlen(trimmed_input) == 0) {
            free(input);
            continue;
        }
        
        // Process any background job completions that occurred while waiting for input
        update_job_status();

        // Check syntax first
        if (!is_valid_syntax(trimmed_input)) {
            fprintf(stderr, "Invalid Syntax!\n");
            if (should_store_command(trimmed_input)) {
                add_to_log(trimmed_input);
            }
            free(input);
            continue;
        }
        
        // Parse input into pipelines
        if (parse_input(trimmed_input, &pipelines, &pipeline_count) == 0) {
            // Store command in log if needed
            if (should_store_command(trimmed_input)) {
                add_to_log(trimmed_input);
            }
            
            // Ensure we print any job completions before executing this command
            update_job_status();

            // If we have more than one pipeline or a single pipeline followed by more commands,
            // suppress background notice for the first background command if there are following commands.
            suppress_bg_notice = (pipeline_count > 1);

            // Execute pipelines
            execute_pipelines(pipelines, pipeline_count);
            
            // Cleanup
            cleanup_pipelines(pipelines, pipeline_count);

            suppress_bg_notice = 0;
        }
        
        free(input);
    }
    
    // Cleanup
    for (int i = 0; i < log_count; i++) {
        free(command_log[i]);
    }
    
    return 0;
}

void init_shell(void) {
    // Get current directory as home directory
    if (getcwd(home_dir, sizeof(home_dir)) == NULL) {
        perror("getcwd");
        exit(1);
    }
    
    strcpy(current_dir, home_dir);
    previous_dir[0] = '\0';  // Initialize as empty - no previous directory yet
    last_foreground_cmd[0] = '\0';
    
    // Get system information
    get_system_info();
}

void get_system_info(void) {
    // Get username from system (not environment variables)
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        strncpy(user_name, pw->pw_name, MAX_HOSTNAME - 1);
        user_name[MAX_HOSTNAME - 1] = '\0';
    } else {
        strcpy(user_name, "unknown");
    }
    
    // Get hostname from system
    if (gethostname(shell_name, MAX_HOSTNAME) != 0) {
        strcpy(shell_name, "unknown");
    }
    shell_name[MAX_HOSTNAME - 1] = '\0';
}

void display_prompt(void) {
    char display_path[MAX_PATH_SIZE];
    
    // Check if current directory is under home directory
    if (strncmp(current_dir, home_dir, strlen(home_dir)) == 0) {
        // Replace home directory path with ~
        if (strlen(current_dir) == strlen(home_dir)) {
            strcpy(display_path, "~");
        } else {
            snprintf(display_path, sizeof(display_path), "~%s", 
                    current_dir + strlen(home_dir));
        }
    } else {
        strcpy(display_path, current_dir);
    }
    
    printf("<%s@%s:%s> ", user_name, shell_name, display_path);
    fflush(stdout);
}

char* get_input(void) {
    char *input = malloc(MAX_INPUT_SIZE);
    if (!input) {
        perror("malloc");
        return NULL;
    }
    
    while (1) {
        errno = 0;
        if (fgets(input, MAX_INPUT_SIZE, stdin) == NULL) {
            if (errno == EINTR) {
                clearerr(stdin);
                continue; // Retry on interrupt
            }
            free(input);
            return NULL; // EOF (Ctrl+D) or other error
        }
        break;
    }
    
    // Remove trailing newline
    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n') {
        input[len - 1] = '\0';
    }
    
    return input;
}
/* ############## LLM Generated Code Ends ################ */
