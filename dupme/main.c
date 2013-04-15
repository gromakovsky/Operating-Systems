#include <stdlib.h>
#include <string.h>
#include <unistd.h>

size_t str_to_size_t(char * str) {
    size_t result = 0;
    size_t i;
    for (i = 0; str[i] != 0; ++i) {
        result *= 10;
        result += str[i] - '0';
    }
    return result;
}

void write_all(int fd, char * buf, size_t count) {
    int written = 0;
    for (; written < count; ) {
        int write_res = write(fd, buf, count - written);
        if (write_res < 0) {
            _exit(EXIT_FAILURE);
        }
        written += write_res;
    }
}

int main(int argc, char * argv[]) {
    if (argc < 2) {
        write_all(1, "Error!", 6);
        _exit(EXIT_FAILURE);
    }
    size_t k = str_to_size_t(argv[1]);
    if (k < 1) {
        write_all(1, "Error!", 6);
        _exit(EXIT_FAILURE);
    }
    ++k;
    char * buf = malloc(k + 1);
    size_t len = 0;

    int eof = 0, ignore = 0;
    while(!eof) {
        size_t r = read(0, buf + len, k - len);
        len += r;
        if (r < 0) {
            return 1;
        }
        if (r == 0) {
            eof = 1;
            buf[len++] = '\n';
        }
        size_t i;
        for (i = 0; i != len; ++i) {
            if (buf[i] == '\n') {
                if (ignore) {
                    ignore = 0;
                } else {
                    write_all(1, buf, i + 1);
                    write_all(1, buf, i + 1);
                }
                len = len - i - 1;
                memmove(buf, buf + i + 1, sizeof(char) * len);
                break; 
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
