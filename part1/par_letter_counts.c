#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ALPHABET_LEN 26

/*
 * Counts the number of occurrences of each letter (case insensitive) in a text
 * file and stores the results in an array.
 * file_name: The name of the text file in which to count letter occurrences
 * counts: An array of integers storing the number of occurrences of each letter.
 *     counts[0] is the number of 'a' or 'A' characters, counts [1] is the number
 *     of 'b' or 'B' characters, and so on.
 * Returns 0 on success or -1 on error.
 */
int count_letters(const char *file_name, int *counts) {
    FILE *file = fopen(file_name, "r");
    if (file == NULL) {
        perror("fopen");
        return -1;
    }

    int character;
    while ((character = fgetc(file)) != EOF) {
        // checks if the character is an alphabet letter
        if (isalpha(character)) {
            // changes the character into lowercase to ensure case insensitivity
            character = tolower(character);

            // Increments the slot in the array corresponding to character
            // Because fgetc() returns the ASCII value of the character and "a" = 97
            // If we do character - 97, it will equal the correct spot in the array
            counts[character - 97]++;
        }
    }

    if (fclose(file) != 0) {
        perror("fclose");
        return -1;
    }
    return 0;
}

/*
 * Processes a particular file(counting occurrences of each letter)
 *     and writes the results to a file descriptor.
 * This function should be called in child processes.
 * file_name: The name of the file to analyze.
 * out_fd: The file descriptor to which results are written
 * Returns 0 on success or -1 on error
 */
int process_file(const char *file_name, int out_fd) {
    int counts[ALPHABET_LEN] = {0};

    if (count_letters(file_name, counts) == -1) {
        return -1;
    }

    // Write the counts array to out_fd (pipe)
    ssize_t bytes_written = write(out_fd, counts, sizeof(counts));
    if (bytes_written != sizeof(counts)) {
        perror("write");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        // No files to consume, return immediately
        return 0;
    }

    // Create a pipe for child processes to write their results
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
        perror("pipe");
        return -1;
    }

    int total_counts[ALPHABET_LEN] = {0};
    int num_files = argc - 1;
    pid_t pids[num_files];    // array to store child PIDs for waitpid() error checking

    // Fork a child to analyze each specified file (names are argv[1], argv[2], ...)
    for (int i = 1; i < argc; i++) {
        pid_t child_pid = fork();
        if (child_pid == -1) {
            perror("fork");
            close(pipe_fds[0]);
            close(pipe_fds[1]);
            return 1;
        } else if (child_pid == 0) {
            // Child process
            // Close read end of pipe
            if (close(pipe_fds[0]) == -1) {
                perror("close");
                close(pipe_fds[1]);
                exit(1);
            }

            // Process file and close write end of pipe
            if (process_file(argv[i], pipe_fds[1]) == -1) {
                close(pipe_fds[1]);
                exit(1);
            }

            if (close(pipe_fds[1]) == -1) {
                perror("close");
                exit(1);
            }

            exit(0);
        } else {
            // Stores child's PID for waitpid() error handling
            pids[i - 1] = child_pid;
        }
    }

    // Only reached by parent process
    // Aggregate all the results together by reading from the pipe in the parent

    // Close write end of pipe
    if (close(pipe_fds[1]) == -1) {
        perror("close");
        close(pipe_fds[0]);
        return -1;
    }

    // Read counts from all children
    for (int i = 0; i < num_files; i++) {
        int temp_counts[ALPHABET_LEN] = {0};
        ssize_t bytes_read = read(pipe_fds[0], temp_counts, sizeof(temp_counts));
        if (bytes_read != sizeof(temp_counts)) {
            perror("read");
            close(pipe_fds[0]);
            return 1;
        }

        for (int j = 0; j < ALPHABET_LEN; j++) {
            total_counts[j] += temp_counts[j];
        }
    }

    // Done reading, close read end
    if (close(pipe_fds[0]) == -1) {
        perror("close");
        return 1;
    }

    // Wait for all children to finish
    for (int i = 0; i < argc - 1; i++) {
        int status;
        if (waitpid(pids[i], &status, 0) == -1) {
            perror("waitpid");
        }
    }

    // Change this code to print out the total count of each letter (case insensitive)
    for (int i = 0; i < ALPHABET_LEN; i++) {
        printf("%c Count: %d\n", 'a' + i, total_counts[i]);
    }
    return 0;
}
