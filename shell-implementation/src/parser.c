/* ############## LLM Generated Code Begins ############## */
#include "../include/shell.h"

char* trim_whitespace(char *str) {
    char *end;
    
    // Trim leading space
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }
    
    if (*str == 0) {
        return str;
    }
    
    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        end--;
    }
    
    end[1] = '\0';
    return str;
}

int is_valid_syntax(char *input) {
    // Simplified syntax validation
    char *trimmed = trim_whitespace(input);
    
    // Empty input is invalid
    if (strlen(trimmed) == 0) {
        return 0;
    }
    
    // Check for invalid character combinations
    char *ptr = trimmed;
    int prev_was_operator = 1; // Start of line counts as operator position
    
    while (*ptr) {
        if (*ptr == '|' || *ptr == '&' || *ptr == ';') {
            if (prev_was_operator) {
                return 0; // Two operators in a row
            }
            prev_was_operator = 1;
        } else if (*ptr == '<' || *ptr == '>') {
            // Redirection operators - skip for now in simple validation
            if (*ptr == '>' && *(ptr+1) == '>') {
                ptr++; // Skip second >
            }
        } else if (*ptr != ' ' && *ptr != '\t') {
            prev_was_operator = 0;
        }
        ptr++;
    }
    
    // Can't end with operator (except &)
    ptr = trimmed + strlen(trimmed) - 1;
    while (ptr >= trimmed && (*ptr == ' ' || *ptr == '\t')) {
        ptr--;
    }
    
    if (ptr >= trimmed && (*ptr == '|' || *ptr == ';')) {
        return 0;
    }
    
    return 1;
}

// Simplified tokenizer for parsing
char** tokenize(char *input, const char *delim, int *count) {
    char **tokens = malloc(MAX_ARGS * sizeof(char*));
    if (!tokens) return NULL;
    
    *count = 0;
    char *token = strtok(input, delim);
    
    while (token && *count < MAX_ARGS - 1) {
        token = trim_whitespace(token);
        if (strlen(token) > 0) {
            tokens[*count] = strdup(token);
            (*count)++;
        }
        token = strtok(NULL, delim);
    }
    
    tokens[*count] = NULL;
    return tokens;
}

void free_tokens(char **tokens, int count) {
    for (int i = 0; i < count; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

Command* parse_command(char *cmd_str) {
    Command *cmd = malloc(sizeof(Command));
    if (!cmd) return NULL;
    
    memset(cmd, 0, sizeof(Command));
    
    // Check for background execution
    char *amp_pos = strrchr(cmd_str, '&');
    if (amp_pos && *(amp_pos + 1) == '\0') {
        cmd->is_background = 1;
        *amp_pos = '\0';
        cmd_str = trim_whitespace(cmd_str);
    }
    
    // Parse redirections - handle multiple redirections (last one wins)
    char *temp_str = strdup(cmd_str);
    char *ptr = temp_str;
    char *cmd_part = malloc(strlen(temp_str) + 1);
    cmd_part[0] = '\0';
    
    while (*ptr) {
        if (*ptr == '<') {
            // Input redirection
            ptr++;
            while (*ptr == ' ') ptr++;
            char *filename = ptr;
            while (*ptr && *ptr != ' ' && *ptr != '>' && *ptr != '<') ptr++;
            char saved = *ptr;
            *ptr = '\0';
            
            // Check if file can be opened for reading (track first error)
            int test_fd = open(filename, O_RDONLY);
            if (test_fd == -1) {
                if (!cmd->input_first_error) {
                    // Remember first failing input redirection
                    if (cmd->input_file) free(cmd->input_file);
                    cmd->input_file = strdup(filename);
                    cmd->input_first_error = 1;
                }
            } else {
                close(test_fd);
                // File can be opened, so store it (last working one wins)
                if (!cmd->input_first_error) {
                    if (cmd->input_file) {
                        free(cmd->input_file);
                    }
                    cmd->input_file = strdup(filename);
                }
            }
            *ptr = saved;
        } else if (*ptr == '>') {
            // Output redirection
            if (*(ptr + 1) == '>') {
                cmd->append_output = 1;
                ptr += 2;
            } else {
                cmd->append_output = 0;
                ptr++;
            }
            while (*ptr == ' ') ptr++;
            char *filename = ptr;
            while (*ptr && *ptr != ' ' && *ptr != '>' && *ptr != '<') ptr++;
            char saved = *ptr;
            *ptr = '\0';
            
            // Check if file can be opened for writing (track first error)
            int flags = O_WRONLY | O_CREAT;
            if (cmd->append_output) {
                flags |= O_APPEND;
            } else {
                flags |= O_TRUNC;
            }
            int test_fd = open(filename, flags, 0644);
            if (test_fd == -1) {
                if (!cmd->output_first_error) {
                    if (cmd->output_file) free(cmd->output_file);
                    cmd->output_file = strdup(filename);
                    cmd->output_first_error = 1;
                    cmd->output_error_append = cmd->append_output;
                }
            } else {
                close(test_fd);
                // File can be opened, so store it (last working one wins)
                if (!cmd->output_first_error) {
                    if (cmd->output_file) {
                        free(cmd->output_file);
                    }
                    cmd->output_file = strdup(filename);
                }
            }
            *ptr = saved;
        } else {
            // Regular command part
            char *start = ptr;
            while (*ptr && *ptr != '<' && *ptr != '>') ptr++;
            if (ptr > start) {
                strncat(cmd_part, start, ptr - start);
            }
        }
    }
    
    // Parse command tokens
    int token_count;
    char **tokens = tokenize(cmd_part, " \t", &token_count);
    
    for (int i = 0; i < token_count && i < MAX_ARGS - 1; i++) {
        cmd->tokens[i] = tokens[i];
    }
    cmd->token_count = token_count;
    
    free(tokens); // Only free the array, not the strings
    free(cmd_part);
    free(temp_str);
    
    return cmd;
}

int parse_input(char *input, Pipeline **pipelines, int *pipeline_count) {
    // Preserve background '&' per segment
    char *input_copy = strdup(input);
    size_t len = strlen(input_copy);
    
    // Temporary buffers
    char **segments = malloc(MAX_ARGS * sizeof(char*));
    int seg_bg[MAX_ARGS];
    int seg_count = 0;
    
    char *buf = malloc(len + 1);
    int bi = 0;
    for (size_t i = 0; i < len; i++) {
        char c = input_copy[i];
        if (c == ';' || c == '&') {
            buf[bi] = '\0';
            char *seg = trim_whitespace(buf);
            if (strlen(seg) > 0) {
                segments[seg_count] = strdup(seg);
                seg_bg[seg_count] = (c == '&');
                seg_count++;
            }
            bi = 0;
        } else {
            buf[bi++] = c;
        }
    }
    // Final segment (no trailing delimiter)
    buf[bi] = '\0';
    char *last = trim_whitespace(buf);
    if (strlen(last) > 0) {
        // Handle trailing '&' within the last segment
        int trailing_bg = 0;
        size_t L = strlen(last);
        if (L > 0 && last[L-1] == '&') {
            size_t k = L - 1;
            while (k > 0 && (last[k] == ' ' || last[k] == '\t' || last[k] == '&')) k--;
            last[k+1] = '\0';
            trailing_bg = 1;
        }
        segments[seg_count] = strdup(trim_whitespace(last));
        seg_bg[seg_count] = trailing_bg;
        seg_count++;
    }
    free(buf);
    
    *pipelines = malloc(seg_count * sizeof(Pipeline));
    *pipeline_count = 0;
    for (int i = 0; i < seg_count; i++) {
        Pipeline *pipeline = &(*pipelines)[*pipeline_count];
        memset(pipeline, 0, sizeof(Pipeline));
        pipeline->is_background = seg_bg[i];
        
        // Split by pipes
        char **pipe_cmds;
        int pipe_count;
        pipe_cmds = tokenize(segments[i], "|", &pipe_count);
        
        for (int j = 0; j < pipe_count; j++) {
            Command *cmd = parse_command(pipe_cmds[j]);
            if (cmd) {
                if (pipeline->is_background) {
                    cmd->is_background = 1; // propagate background to commands
                }
                pipeline->commands[pipeline->command_count] = cmd;
                pipeline->command_count++;
            }
        }
        
        if (pipeline->command_count > 0) {
            (*pipeline_count)++;
        }
        
        free_tokens(pipe_cmds, pipe_count);
        free(segments[i]);
    }
    free(segments);
    free(input_copy);
    return 0;
}

void cleanup_pipelines(Pipeline *pipelines, int pipeline_count) {
    for (int i = 0; i < pipeline_count; i++) {
        Pipeline *pipeline = &pipelines[i];
        for (int j = 0; j < pipeline->command_count; j++) {
            Command *cmd = pipeline->commands[j];
            for (int k = 0; k < cmd->token_count; k++) {
                free(cmd->tokens[k]);
            }
            free(cmd->input_file);
            free(cmd->output_file);
            free(cmd);
        }
    }
    free(pipelines);
}
/* ############## LLM Generated Code Ends ################ */
