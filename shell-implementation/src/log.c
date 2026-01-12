/* ############## LLM Generated Code Begins ############## */
#include "../include/shell.h"

extern char *command_log[MAX_LOG_COMMANDS];
extern int log_count;
extern int log_start;

static char log_path[MAX_PATH_SIZE];

static void compute_log_path(void) {
    const char *hd = getenv("HOME");
    if (!hd || !*hd) {
        struct passwd *pw = getpwuid(getuid());
        if (pw && pw->pw_dir) {
            hd = pw->pw_dir;
        } else {
            hd = ".";
        }
    }
    snprintf(log_path, sizeof(log_path), "%s/.shell_log", hd);
}

void init_log(void) {
    compute_log_path();
    // Try to load log from file in HOME
    FILE *log_file = fopen(log_path, "r");
    if (log_file) {
        char line[MAX_INPUT_SIZE];
        while (fgets(line, sizeof(line), log_file) && log_count < MAX_LOG_COMMANDS) {
            // Remove newline
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') {
                line[len - 1] = '\0';
            }
            
            command_log[(log_start + log_count) % MAX_LOG_COMMANDS] = strdup(line);
            log_count++;
        }
        fclose(log_file);
    }
}

void save_log(void) {
    if (!*log_path) compute_log_path();
    FILE *log_file = fopen(log_path, "w");
    if (log_file) {
        for (int i = 0; i < log_count; i++) {
            int index = (log_start + i) % MAX_LOG_COMMANDS;
            fprintf(log_file, "%s\n", command_log[index]);
        }
        fclose(log_file);
    }
}

void add_to_log(char *command) {
    // Work on a trimmed copy for consistency
    char *trimmed = strdup(trim_whitespace(command));
    // Don't store if identical to previous command
    if (log_count > 0) {
        int last_index = (log_start + log_count - 1) % MAX_LOG_COMMANDS;
        if (strcmp(command_log[last_index], trimmed) == 0) {
            free(trimmed);
            return;
        }
    }
    
    if (log_count < MAX_LOG_COMMANDS) {
        // Log not full yet
        command_log[(log_start + log_count) % MAX_LOG_COMMANDS] = trimmed;
        log_count++;
    } else {
        // Log is full, overwrite oldest
        free(command_log[log_start]);
        command_log[log_start] = trimmed;
        log_start = (log_start + 1) % MAX_LOG_COMMANDS;
    }
    
    save_log();
}

int should_store_command(char *command) {
    // Parse command to check if it contains 'log' as atomic command
    char *cmd_copy = strdup(command);
    char *token = strtok(cmd_copy, " \t|;&");
    
    while (token) {
        token = trim_whitespace(token);
        if (strcmp(token, "log") == 0) {
            free(cmd_copy);
            return 0; // Don't store
        }
        token = strtok(NULL, " \t|;&");
    }
    
    free(cmd_copy);
    return 1; // Store
}

int log_command(char **args, int arg_count) {
    if (arg_count == 1) {
        // Print log
        print_log();
        return 0;
    }
    
    if (arg_count == 2 && strcmp(args[1], "purge") == 0) {
        // Purge log
        purge_log();
        return 0;
    }
    
    if (arg_count == 3 && strcmp(args[1], "execute") == 0) {
        // Execute command from log
        int index = atoi(args[2]);
        if (index < 1 || index > log_count) {
            fprintf(stderr, "log: Invalid Syntax!\n");
            return 1;
        }
        execute_from_log(index);
        return 0;
    }
    
    fprintf(stderr, "log: Invalid Syntax!\n");
    return 1;
}

void print_log(void) {
    for (int i = 0; i < log_count; i++) {
        int index = (log_start + i) % MAX_LOG_COMMANDS;
        printf("%s\n", command_log[index]);
    }
}

void execute_from_log(int index) {
    if (index < 1 || index > log_count) {
        return;
    }
    
    // Map index: 1 -> most recent, 2 -> second most recent, ...
    int log_index = (log_start + log_count - index) % MAX_LOG_COMMANDS;
    char *command = command_log[log_index];
    
    // Parse and execute the command
    Pipeline *pipelines;
    int pipeline_count;
    
    if (!is_valid_syntax(command)) {
        fprintf(stderr, "Invalid Syntax!\n");
        return;
    }
    
    char *cmd_copy = strdup(command);
    if (parse_input(cmd_copy, &pipelines, &pipeline_count) == 0) {
        if (should_store_command(command)) {
            add_to_log(command);
        }
        execute_pipelines(pipelines, pipeline_count);
        cleanup_pipelines(pipelines, pipeline_count);
    }
    free(cmd_copy);
}

void purge_log(void) {
    for (int i = 0; i < log_count; i++) {
        free(command_log[i]);
    }
    log_count = 0;
    log_start = 0;
    
    // Remove log file
    if (!*log_path) compute_log_path();
    remove(log_path);
}
/* ############## LLM Generated Code Ends ################ */
