/* ############## LLM Generated Code Begins ############## */
#ifndef SHELL_H
#define SHELL_H

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define MAX_INPUT_SIZE 4096
#define MAX_ARGS 100
#define MAX_PATH_SIZE 1024
#define MAX_JOBS 100
#define MAX_LOG_COMMANDS 15
#define MAX_HOSTNAME 256

// Global variables
extern char home_dir[MAX_PATH_SIZE];
extern char current_dir[MAX_PATH_SIZE];
extern char previous_dir[MAX_PATH_SIZE];
extern char shell_name[MAX_HOSTNAME];
extern char user_name[MAX_HOSTNAME];
extern char last_foreground_cmd[MAX_INPUT_SIZE];
extern int suppress_bg_notice;

// Structure definitions
typedef struct {
    char *tokens[MAX_ARGS];
    int token_count;
    char *input_file;
    char *output_file;
    int append_output;
    // Redirection error precedence handling
    int input_first_error;           // 1 if input_file is the first failing redirect
    int output_first_error;          // 1 if output_file is the first failing redirect
    int output_error_append;         // append flag for the first failing output redirect
    int is_background;
} Command;

typedef struct {
    Command *commands[MAX_ARGS];
    int command_count;
    int is_background;
} Pipeline;

typedef struct {
    int job_id;
    pid_t pid;
    char *command;
    int is_background;
    int is_stopped;
    time_t start_time;
} Job;

// Function declarations
void init_shell(void);
void display_prompt(void);
char* get_input(void);
int parse_input(char *input, Pipeline **pipelines, int *pipeline_count);
int is_valid_syntax(char *input);
void execute_pipelines(Pipeline *pipelines, int pipeline_count);
void cleanup_pipelines(Pipeline *pipelines, int pipeline_count);
int execute_shell_intrinsic(Command *cmd);
char* reconstruct_command(Command *cmd);
char* reconstruct_pipeline_command(Pipeline *pipeline);

// Shell intrinsics
int hop_command(char **args, int arg_count);
int reveal_command(char **args, int arg_count);
int log_command(char **args, int arg_count);
int activities_command(char **args, int arg_count);
int fg_command(char **args, int arg_count);
int bg_command(char **args, int arg_count);
int ping_command(char **args, int arg_count);

// Command execution
int execute_command(Command *cmd);
int execute_pipeline(Pipeline *pipeline);
void handle_redirection(Command *cmd, int *input_fd, int *output_fd);
void restore_fds(int saved_stdin, int saved_stdout);
void set_last_foreground_cmd(const char *s);

// Job control
void add_job(pid_t pid, char *command, int is_background);
void remove_job(pid_t pid);
void update_job_status(void);
void print_job_status(pid_t pid, int status);
Job* find_job_by_id(int job_id);
Job* find_job_by_pid(pid_t pid);

// Signal handling
void setup_signals(void);
void sigchld_handler(int sig);
void sigint_handler(int sig);
void sigtstp_handler(int sig);

// Log management
void init_log(void);
void add_to_log(char *command);
void print_log(void);
void execute_from_log(int index);
void purge_log(void);
int should_store_command(char *command);

// Utility functions
char* trim_whitespace(char *str);
void get_system_info(void);
char* resolve_path(char *path);
int is_shell_intrinsic(char *command);
void print_error(char *message);
char** tokenize(char *input, const char *delim, int *count);
void free_tokens(char **tokens, int count);
Command* parse_command(char *cmd_str);

#endif
/* ############## LLM Generated Code Ends ################ */
