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
        if (write_res < 0) {
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

int main(int argc, char * argv[]) {
    char * buffer;
    char delim = '\n';
    char * old_string;

    if (argc != 2) {
        write(STDOUT_FILENO, "Error, bad args count", strlen("Error, bad args count"));
        _exit(EXIT_FAILURE);
    }
    char * file_name = argv[1];

    size_t buffer_size = 4096;
    buffer = my_malloc(buffer_size);
    size_t len = 0;
    size_t old_len = -1;
//    size_t old_string_len = 0;
    size_t old_string_size = 4096;
    old_string = my_malloc(old_string_size);
    strcpy(old_string, "\0");

    int fd = open(file_name, O_RDONLY);
    if (fd < 0) {
        _exit(EXIT_FAILURE);
    }
    
    int eof = 0;
    while (!eof) {
        int r = read(fd, buffer + len, buffer_size - len);
        if (r < 0) {
            _exit(EXIT_FAILURE);
        }
        eof = !r;
        old_len = len;
        len += r;
        if (len == buffer_size) {
            buffer_size *= 2;
            buffer = realloc(buffer, buffer_size);
        }
        if (len <= buffer_size / 4) {
            buffer_size /= 2;
            buffer = realloc(buffer, buffer_size);
        }
        size_t i;
        for (i = old_len; i < len; ++i) {
            if (buffer[i] == delim) {
                buffer[i] = '\0';

                if (strcmp(old_string, buffer) < 0) {
                    write_all(STDOUT_FILENO, buffer, i);
                    write(STDOUT_FILENO, "\n", 1);
                }

                if (old_string_size < len) {
                    old_string_size = len;
                    old_string = realloc(old_string, old_string_size);
                }
                strcpy(old_string, buffer);
                memmove(buffer, buffer + i + 1, (len - i));
                len -= (i + 1);
                i = -1;
            }
        }
    }

    free(buffer);
    free(old_string);
    close(fd);
    _exit(EXIT_SUCCESS);
}
