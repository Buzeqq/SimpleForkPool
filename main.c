#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <wait.h>
#include <sys/ioctl.h>

#define NO_OF_PROCESSES 7
#define P_READ 0
#define P_WRITE 1

typedef struct fork_pool_t {
    pid_t child_processes[NO_OF_PROCESSES];
    int input[2];
    int output[2];

    bool (*task)(int);
} fork_pool_t;

void initialise_fork_pool(fork_pool_t *fork_pool) {
    if (pipe(fork_pool->input) != 0 || pipe(fork_pool->output) != 0) {
        perror("Could not create pipes!\n");
        exit(errno);
    }
    fork_pool->task = NULL;
}

void map(fork_pool_t *fork_pool, int *data, size_t data_size, bool (*task)(int)) {
    fork_pool->task = (bool (*)(int)) task;
    for (int i = 0; i < data_size; i++) {
        write(fork_pool->input[P_WRITE], &data[i], sizeof(data[i]));
    }
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
    close(fork_pool->input[P_WRITE]);

    for (size_t i = 0; i < NO_OF_PROCESSES; i++) {
        pid_t *child = &fork_pool->child_processes[i];

        if ((*child = fork()) < 0) {
            perror("Failed to create child process!");
            exit(errno);
        } else if (*child == 0) { // child process
            printf("Process %d: running task!\n", getpid());
            int bytes_to_read = 0;
            ioctl(fork_pool->input[P_READ], FIONREAD, &bytes_to_read);

            // run task
            int data = 0;
            int counter = 0;
            while (bytes_to_read > 0) {
                read(fork_pool->input[P_READ], &data, sizeof(data));
                if (fork_pool->task(data)) {
                    write(fork_pool->output[P_WRITE], &data, sizeof(data));
                }
                ioctl(fork_pool->input[P_READ], FIONREAD, &bytes_to_read);
                counter++;
            }

            close(fork_pool->input[P_READ]);
            printf("No more data to read, data read: %d, last read data: %d, pid: %d\n", counter, data, getpid());
            exit(0);
        }
    }
    // parent process
    // wait for child processes
    for (int i = 0; i < NO_OF_PROCESSES; i++) {
        waitpid(fork_pool->child_processes[i], NULL, 0);
    }

    printf("Parent process: %d waited for all the child process.\n", getpid());
    close(fork_pool->output[P_WRITE]);
    int bytes_to_read;

    ioctl(fork_pool->output[P_READ], FIONREAD, &bytes_to_read);
    int data;
    int counter = 0;
    while (bytes_to_read > 0) {
        read(fork_pool->output[P_READ], &data, sizeof(data));
        printf("%d ", data);
        ioctl(fork_pool->output[P_READ], FIONREAD, &bytes_to_read);
        counter++;
    }
    printf("\nTotal of prime numbers: %d\n", counter);
    close(fork_pool->output[P_READ]);
}

int main() {
    fork_pool_t fork_pool;

    initialise_fork_pool(&fork_pool);
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
