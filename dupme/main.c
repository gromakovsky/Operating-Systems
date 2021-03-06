#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

void action(int arg1, void * arg2, size_t arg3) {
    write_all(arg1, arg2, arg3);
    write_all(arg1, arg2, arg3);
}

const char DELIM = '\n';

int main(int argc, char * argv[]) {
    if (argc < 2) {
        write_all(STDERR_FILENO, "Error!", 6);
        _exit(EXIT_FAILURE);
    }
    size_t k = atoi(argv[1]) + 1;
    if (k < 2) {
        write_all(STDERR_FILENO, "Error!", 6);
        _exit(EXIT_FAILURE);
    }
    char * buf = malloc(k + 1);
    if (buf == NULL) {
        _exit(EXIT_FAILURE);
    }
    size_t len = 0;

    int eof = 0;
    int ignore = 0;
    while (!eof) {
        int r = read(STDIN_FILENO, buf + len, k - len);
        if (r < 0) {
            _exit(EXIT_FAILURE);
        }
        if (r == 0) {
            eof = 1;
            buf[len++] = DELIM;
        }
        len += r;

        size_t i;
        for (i = 0; i != len; ++i) {
            if (buf[i] == DELIM) {
                if (ignore) {
                    ignore = 0;
                } else {
                    action(STDOUT_FILENO, buf, i + 1);
                }
                len = len - i - 1;
                memmove(buf, buf + i + 1, len);
                i = -1;
            }
        }
        if (i == k) {
            ignore = 1;
            len = 0;
        }
    }

    free(buf);
    _exit(EXIT_SUCCESS);
}
