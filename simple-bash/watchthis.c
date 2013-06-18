#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>

const char * const FIFO1 = "/tmp/watchthis_FIFO1";
const char * const FIFO2 = "/tmp/watchthis_FIFO2";

const size_t CAPACITY = 4096;

void my_exit(int status) {
    remove(FIFO1);
    remove(FIFO2);
    _exit(status);
}

void * my_malloc(size_t size) {
    void * res = malloc(size);
    if (res == NULL) {
        perror("malloc");
        my_exit(EXIT_FAILURE);
    }
    return res;
}

void my_read(int fd, char * buf, size_t * len) {
    int r;
    while ((r = read(fd, buf + *len, CAPACITY - *len)) > 0) {
        *len += r;
    }
    if (r == -1) {
        perror("read");
        my_exit(EXIT_FAILURE);
    }
}

void my_close(int fd) {
    if (close(fd) == -1) {
        perror("close");
        my_exit(EXIT_FAILURE);
    }
}

void my_dup2(int oldfd, int newfd) {
    if (dup2(oldfd, newfd) == -1) {
        perror("dup2");
        my_exit(EXIT_FAILURE);
    }
}

void write_all(int fd, const char * buf, size_t count) {
    size_t written = 0;
    while (written < count) {
        int write_res = write(fd, buf, count - written);
        if (write_res == -1) {
            perror("write");
            my_exit(EXIT_FAILURE);
        }
        written += write_res;
    }
}

void my_run(char * argv[], int * pipefd) {
    if (fork()) {
        wait(NULL);
    } else {
        my_dup2(pipefd[1], STDOUT_FILENO);
        my_close(pipefd[0]);
        my_close(pipefd[1]);
        execvp(argv[0], argv);
        perror("execvp");
        my_exit(EXIT_FAILURE);
    }
}

void run_diff(const char * new_buffer, size_t new_len, const char * old_buffer, size_t old_len) {
    if (fork()) {
        int fifo1_fd = open(FIFO1, O_WRONLY);
        int fifo2_fd = open(FIFO2, O_WRONLY);
        if ((fifo1_fd == -1) || (fifo2_fd == -1)) {
            perror("open");
            my_exit(EXIT_FAILURE);
        }

        write_all(fifo1_fd, new_buffer, new_len);
        write_all(fifo2_fd, old_buffer, old_len);
        my_close(fifo1_fd);
        my_close(fifo2_fd);
     
        wait(NULL);
    } else {
        execlp("diff", "diff", "-u", FIFO1, FIFO2, NULL);
        perror("execlp");
        my_exit(EXIT_FAILURE);
    }
}

int main(int argc, char * argv[]) {
    if (argc < 3) {
        write_all(STDERR_FILENO, "Error, not enough args\n", strlen("Error, not enough args\n"));
        _exit(EXIT_FAILURE);
    }

    if (atoi(argv[1]) <= 0) {
        write_all(STDERR_FILENO, "Error, interval must be positive\n", strlen("Error, interval must be positive\n"));
        _exit(EXIT_FAILURE);
    }
    const unsigned int interval = atoi(argv[1]);

    if (mkfifo(FIFO1, S_IRUSR | S_IWUSR) || mkfifo(FIFO2, S_IRUSR | S_IWUSR)) {
        perror("mkfifo");
        my_exit(EXIT_FAILURE);
    }

    signal(SIGINT, my_exit);
    signal(SIGTERM, my_exit);
    signal(SIGSEGV, my_exit);

    int pipefd[2];    
    char * new_buffer = my_malloc(CAPACITY);
    char * old_buffer = my_malloc(CAPACITY);
    size_t len = 0;
    size_t old_len = 0;
    while (1) {
        if (pipe(pipefd) == -1) {
            perror("pipe");
            my_exit(EXIT_FAILURE);
        }
        my_run(argv + 2, pipefd);
        my_dup2(pipefd[0], STDIN_FILENO);
        my_close(pipefd[1]);
        my_read(pipefd[0], new_buffer, &len);
        my_close(pipefd[0]);

        write_all(STDOUT_FILENO, new_buffer, len);
        if (len == CAPACITY - 1) {
            write_all(STDERR_FILENO, "Too small buffer", strlen("Too small buffer"));
            my_exit(EXIT_FAILURE);
        }
        new_buffer[len] = '\0';

        if (strcmp(new_buffer, old_buffer) && old_len) {
            run_diff(new_buffer, len, old_buffer, old_len);
        }

        old_len = len;
        len = 0;
        char * tmp = new_buffer;
        new_buffer = old_buffer;
        old_buffer = tmp;

        unsigned int left_to_sleep = interval;
        while (left_to_sleep) {
            left_to_sleep = sleep(left_to_sleep);
        }
    }

    free(old_buffer);
    free(new_buffer);
    my_exit(EXIT_SUCCESS);
}
