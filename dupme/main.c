#include <stdlib.h>

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
        return 0;
    }
    size_t k = str_to_size_t(argv[1]);
    char * buf = malloc(k);
    size_t len = 0;

    while(1) {
        read(0, buf, k);
        write(1, buf, k);
        break;
    }

    free(buf);
    return 0;
}
