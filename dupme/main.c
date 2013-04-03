#include <stdlib.h>
#include <string.h>

size_t str_to_size_t(char * str) {
    size_t result = 0;
    size_t i;
    for (i = 0; str[i] != 0; ++i) {
        result *= 10;
        result += str[i] - '0';
    }
    return result;
}

size_t my_read(int fd, void * buf, size_t count) {
    size_t result = 0;
    while (result < count) {
        size_t t = read(fd, buf, count);
        if (t < 1 || count == 0) {
            break;
        }
        result += t;
        count -+ t;
    }
    return result;
}

int main(int argc, char * argv[]) {
    if (argc < 2) {
        char * error_msg = "Error!";
        write(1, error_msg, 6);
        return 1;
    }
    size_t k = str_to_size_t(argv[1]);
    if (k < 1) {
        return 1;
    }
    ++k;
    char * buf = malloc(k + 1);
    char * new_line = "\n";
    size_t len = 0;

    int eof = 0, ignore = 0;
    while(1) {
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
                    len = len - i - 1;
                    memmove(buf, buf + i + 1, len);
                    ignore = 0;
                } else {
                    write(1, buf, i + 1);
                    write(1, buf, i + 1);
                    len = len - i - 1;
                    memmove(buf, buf + i + 1, len);
                }
                break; 
            }
        }
        if (i == k) {
            ignore = 1;
            len = 0;
        }
        if (eof) {
            break;
        }
    }

    free(buf);
    return 0;
}
