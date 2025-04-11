#include "swish_funcs.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "string_vector.h"

#define MAX_ARGS 10

/*
 * Helper function to run a single command within a pipeline. You should make
 * make use of the provided 'run_command' function here.
 * tokens: String vector containing the tokens representing the command to be
 * executed, possible redirection, and the command's arguments.
 * pipes: An array of pipe file descriptors.
 * n_pipes: Length of the 'pipes' array
 * in_idx: Index of the file descriptor in the array from which the program
 *         should read its input, or -1 if input should not be read from a pipe.
 * out_idx: Index of the file descriptor in the array to which the program
 *          should write its output, or -1 if output should not be written to
 *          a pipe.
 * Returns 0 on success or -1 on error.
 */
int run_piped_command(strvec_t *tokens, int *pipes, int n_pipes, int in_idx, int out_idx) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Error forking");
        return -1;
    } else if (pid == 0) {
        // Set up input redirection
        if (in_idx != -1) {
            if (dup2(pipes[in_idx], STDIN_FILENO) == -1) {
                perror("Error dup2 input");
                exit(1);
            }
        }

        // Set up output redirection
        if (out_idx != -1) {
            if (dup2(pipes[out_idx], STDOUT_FILENO) == -1) {
                perror("Error dup2 output");
                exit(1);
            }
        }

        // Close all pipe fds in child
        for (int i = 0; i < n_pipes * 2; ++i) {
            close(pipes[i]);
        }

        run_command(tokens);
        perror("Exec failed");
        exit(1);
    }

    return 0;
}

int run_pipelined_commands(strvec_t *tokens) {
    int num_tokens = tokens->length;
    int num_commands = strvec_num_occurrences(tokens, "|") + 1;
    int num_pipes = num_commands - 1;

    // Set up pipes
    int pipes[2 * num_pipes];
    for (int i = 0; i < num_pipes; ++i) {
        if (pipe(&pipes[2 * i]) == -1) {
            perror("Error creating pipe");
            return -1;
        }
    }

    int cmd_start = 0;
    int cmd_idx = 0;

    for (int i = 0; i <= num_tokens; ++i) {
        if (i == num_tokens || ((tokens->data[i][0] == '|' && tokens->data[i][1] == '\0'))) {
            // Slice command tokens from cmd_start to i
            strvec_t cmd_tokens;
            if (strvec_slice(tokens, &cmd_tokens, cmd_start, i) == -1) {
                fprintf(stderr, "strvec_slice failed\n");
                return -1;
            }

            int in_idx;
            if (cmd_idx == 0) {
                in_idx = -1;
            } else {
                in_idx = 2 * (cmd_idx - 1);
            }

            int out_idx;
            if (cmd_idx == num_commands - 1) {
                out_idx = -1;
            } else {
                out_idx = 2 * cmd_idx + 1;
            }

            if (run_piped_command(&cmd_tokens, pipes, num_pipes, in_idx, out_idx) == -1) {
                strvec_clear(&cmd_tokens);
                return -1;
            }

            // Free memory
            strvec_clear(&cmd_tokens);
            cmd_start = i + 1;
            cmd_idx++;
        }
    }

    // Close all pipe file descriptors in parent
    for (int i = 0; i < 2 * num_pipes; ++i) {
        close(pipes[i]);
    }

    // Wait for all children
    for (int i = 0; i < num_commands; ++i) {
        int status;
        if (wait(&status) == -1) {
            perror("Error waiting for child");
        }
    }

    return 0;
}
