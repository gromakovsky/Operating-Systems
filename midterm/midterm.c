#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void write_all(int fd, const char * buf, size_t count) {
    size_t written = 0;
    while (written < count) {
        int write_res = write(fd, buf, count - written);
        if (write_res == -1) {
            perror("write");
            _exit(EXIT_FAILURE);
        }
        written += write_res;
    }
}

void * my_malloc(size_t size) {
    void * res = malloc(size);
    if (res == NULL) {
        _exit(EXIT_FAILURE);
    }
    return res;
}

void * my_realloc(void * ptr, size_t size) {
    void * res = realloc(ptr, size);
    if (res == NULL) {
        _exit(EXIT_FAILURE);
    }
    return res;
}

void my_close(int fd) {
    if (close(fd) == -1) {
        perror("close");
        _exit(EXIT_FAILURE);
    }
}

const size_t CAPACITY = 4096;
const char DELIMITER = '\n';

int main(int argc, char * argv[]) {
    char * buffer;
    char * old_string;

    if (argc != 2) {
        write_all(STDERR_FILENO, "Error, bad arguments count", strlen("Error, bad arguments count"));
        _exit(EXIT_FAILURE);
    }
    const char * const FILENAME = argv[1];

    buffer = my_malloc(CAPACITY);
    size_t len = 0;
    size_t old_len;
    old_string = my_malloc(CAPACITY);
    old_string[0] = '\0';

    int fd = open(FILENAME, O_RDONLY);
    if (fd == -1) {
        perror("open");
        _exit(EXIT_FAILURE);
    }
    
    int eof = 0;
    while (!eof) {
        int r = read(fd, buffer + len, CAPACITY - len);
        if (r == -1) {
            perror("read");
            _exit(EXIT_FAILURE);
        }
        
        eof = !r;
        old_len = len;
        len += r;
        if (len == CAPACITY) {
            write_all(STDERR_FILENO, "Error, not enough capacity", strlen("Error, not enough capacity"));
        }

        size_t i;
        for (i = old_len; i < len; ++i) {
            if (buffer[i] == DELIMITER) {
                buffer[i] = '\0';

                if (strcmp(old_string, buffer) < 0) {
                    write_all(STDOUT_FILENO, buffer, i);
                    write_all(STDOUT_FILENO, "\n", 1);
                }

                strcpy(old_string, buffer);
                memmove(buffer, buffer + i + 1, (len - i));
                len -= (i + 1);
                i = (size_t) -1;
            }
        }
    }

    free(buffer);
    free(old_string);
    my_close(fd);
    _exit(EXIT_SUCCESS);
}
