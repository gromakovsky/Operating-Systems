#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void write_all(int fd, char * buffer, size_t count) {
    int written = 0;
    for (; written < count; ) {
        int write_res = write(fd, buffer, count - written);
        if (write_res < 0) {
            _exit(EXIT_FAILURE);
        }
        written += write_res;
    }
    write(fd, "\n", 1);
}

int main(int argc, char * argv[]) {
    char * buffer;
    char delim = '\n';
    char * old_string;

    if (argc != 2) {
        _exit(EXIT_FAILURE);
    }
    char * file_name = argv[1];

    size_t buffer_size = 4096;
    buffer = malloc(buffer_size * sizeof(char));
    size_t len = 0;
    size_t old_len = -1;
    size_t old_string_len = 0;
    size_t old_string_size = 4096;
    old_string = malloc(old_string_size * sizeof(char));
    strcpy(old_string, "\0");

    int fd = open(file_name, S_IRUSR);
    if (fd < 0) {
        _exit(EXIT_FAILURE);
    }
    
    int r;
    while ((r = read(fd, buffer + len, buffer_size - len)) > 0) {
        old_len = len;
        len += r;
        if (len == buffer_size) {
            buffer_size *= 2;
            buffer = realloc(buffer, buffer_size * sizeof(char));
        }
        if (len <= buffer_size / 4) {
            buffer_size /= 2;
            buffer = realloc(buffer, buffer_size * sizeof(char));
        }
        size_t i;
        for (i = old_len; i < len; ++i) {
            if (buffer[i] == delim) {
                buffer[i] = '\0';

                if (strcmp(old_string, buffer) < 0) {
                    write_all(STDOUT_FILENO, buffer, i);
                }

                if (old_string_size < len) {
                    old_string_size = len;
                    old_string = realloc(old_string, old_string_size * sizeof(char));
                }
                strcpy(old_string, buffer);
                memmove(buffer, buffer + i + 1, (len - i) * sizeof(char));
                len -= (i + 1);
                i = 0;
            }
        }
    }
    if (r < 0) {
        _exit(EXIT_FAILURE);
    }

    free(buffer);
    free(old_string);
    close(fd);
    _exit(EXIT_SUCCESS);
}
