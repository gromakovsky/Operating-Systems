#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

char * FIFO1 = "/tmp/watchthis_FIFO1";
char * FIFO2 = "/tmp/watchthis_FIFO2";

char * new_buffer;
char * old_buffer;
size_t buffer_size = 4;

void my_exit(int status) {
    remove(FIFO1);
    remove(FIFO2);
    _exit(status);
}

void my_read(int fd, size_t * len) {
    int r;
    while ((r = read(fd, new_buffer + *len, buffer_size - *len)) > 0) {
        *len += r;
        if (*len == buffer_size) {
            buffer_size *= 2;
            new_buffer = realloc(new_buffer, buffer_size);
            old_buffer = realloc(old_buffer, buffer_size);
        }
    }
}

void write_all(int fd, char * buffer, size_t count) {
    int written = 0;
    for (; written < count; ) {
        int write_res = write(fd, buffer, count - written);
        if (write_res < 0) {
            _exit(EXIT_FAILURE);
        }
        written += write_res;
    }
}

void my_run(char * argv[], int * pipefd) {
    if (fork()) {
        wait(NULL);
    } else {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        execvp(argv[0], argv);
        _exit(EXIT_SUCCESS);
    }
}

void run_diff(size_t len, size_t old_len) {
    if (fork()) {
        int fifo1_fd = open(FIFO1, O_WRONLY);
        int fifo2_fd = open(FIFO2, O_WRONLY);
    
        write_all(fifo1_fd, new_buffer, len);
        write_all(fifo2_fd, old_buffer, old_len);
        close(fifo1_fd);
        close(fifo2_fd);

        wait(NULL);
    } else {
        execlp("diff", "diff", "-u", FIFO1, FIFO2, NULL);
        _exit(EXIT_SUCCESS);
    }
}

int main(int argc, char * argv[]) {
    if (argc < 2) {
        write_all(1, "Error, not enough args\n", 23);
        _exit(EXIT_SUCCESS);
    }

    int interval = atoi(argv[1]);

    if (mkfifo(FIFO1, S_IRUSR | S_IWUSR) || mkfifo(FIFO2, S_IRUSR | S_IWUSR)) {
        my_exit(EXIT_FAILURE);
    }

    signal(SIGINT, my_exit);
    signal(SIGTERM, my_exit);

    int pipefd[2];    
    new_buffer = malloc(buffer_size);
    old_buffer = malloc(buffer_size);
    size_t len = 0;
    size_t old_len = 0;
    while (1) {
        pipe(pipefd);
        my_run(argv + 2, pipefd);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[1]);
        my_read(pipefd[0], &len);
        close(pipefd[0]);

        write_all(STDOUT_FILENO, new_buffer, len);

        if (strcmp(new_buffer, old_buffer) && old_len) {
            run_diff(len, old_len);
        }

        old_len = len;
        len = 0;
        char * tmp = new_buffer;
        new_buffer = old_buffer;
        old_buffer = tmp;

        sleep(interval);
    }
    free(old_buffer);
    free(new_buffer);
    my_exit(EXIT_SUCCESS);
}
