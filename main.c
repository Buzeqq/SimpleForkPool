#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <wait.h>

#define NO_OF_PROCESSES 7
#define P_READ 2 * i + 0
#define P_WRITE 2 * i + 1

typedef struct fork_pool_t {
    pid_t child_processes[NO_OF_PROCESSES];
    int* data;
    size_t size_of_data;
    bool (*task)(int);
} fork_pool_t;

void map(fork_pool_t* fork_pool, int* data, size_t data_size, bool (*task)(int)) {
    fork_pool->data = data;
    fork_pool->size_of_data = data_size;
    fork_pool->task = (bool (*)(int)) task;
}

bool is_prime(int n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;

    for (int i = 5; i * i <= n; i = i + 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }

    return true;
}

void run(fork_pool_t* fork_pool) {
    int input_pipes[2 * NO_OF_PROCESSES];
    int output_pipes[2 * NO_OF_PROCESSES];

    for (int i = 0; i < NO_OF_PROCESSES; i++) {
        if (pipe(&input_pipes[2 * i]) == -1) {
            perror("Could not create pipe!");
            exit(errno);
        }

        if (pipe(&output_pipes[2 * i]) == -1) {
            perror("Could not create pipe!");
            exit(errno);
        }
    }
    size_t bytes_written = 0;
    for (size_t i = 0; i < NO_OF_PROCESSES; i++) {
        pid_t* child = &fork_pool->child_processes[i];

        if ((*child = fork()) < 0) {
            perror("Failed to create child process!");
            exit(errno);
        } else if (*child > 0) { // parent process
            close(input_pipes[P_READ]);// close input read end for parent process

            // write
            size_t data_offset = (i * fork_pool->size_of_data) / NO_OF_PROCESSES;
            size_t rest = 0;
            if (i == NO_OF_PROCESSES - 1) {
                rest = fork_pool->size_of_data % NO_OF_PROCESSES;
            }
            bytes_written += write(input_pipes[2 * i + 1],
                                   fork_pool->data + data_offset,
                                   (fork_pool->size_of_data / NO_OF_PROCESSES + rest) * sizeof(int));

            close(input_pipes[P_WRITE]); // close input write end for parent process
        } else { // child process
            printf("Process %d: running task!\n", getpid());
            close(input_pipes[P_WRITE]); // close input write end for child process
            close(output_pipes[P_READ]); // close output read end for child process
            // run task
            int data = 0;
            int counter = 0;
            while (read(input_pipes[P_READ], &data, sizeof(data)) > 0) {
                if (fork_pool->task(data)) {
                    write(output_pipes[P_WRITE], &data, sizeof(data));
                }
                counter++;
            }
            printf("No more data to read, data read: %d, last read data: %d, pid: %d\n", counter, data, getpid());
            close(input_pipes[P_READ]); // close input read end for child process
            close(output_pipes[P_WRITE]); // close output write end for child process
            exit(0);
        }
    }
    printf("Bytes written: %zu\n", bytes_written);

    // wait for child processes
    for (int i = 0; i < NO_OF_PROCESSES; i++) {
        waitpid(fork_pool->child_processes[i], NULL, 0);
    }

    printf("Parent process: %d waited for all the child process.\n", getpid());
    for (int i = 0; i < NO_OF_PROCESSES; i++) {
        close(output_pipes[P_WRITE]);
        int data = 0;
        while (read(output_pipes[P_READ], &data, sizeof(data)) > 0) {
            printf("%d ", data);
        }
        close(output_pipes[P_READ]);
    }
}

int main() {
    fork_pool_t fork_pool;

    printf("Enter the range: ");
    int min = 0, max = 0;
    scanf("%d %d", &min, &max);
    if (max < min) return 1;

    int size_of_array = max - min + 1;
    int* numbers = (int*)calloc(size_of_array, sizeof(int));

    for (int i = 0; i < size_of_array; i++) {
        numbers[i] = min + i;
    }

    // map
    map(&fork_pool, numbers, size_of_array, is_prime);

    // run
    run(&fork_pool);

    free(numbers);
    return 0;
}
