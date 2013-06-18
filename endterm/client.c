#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pty.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <sys/stat.h>

const char * const SERVICE = "8822";
const size_t CAPACITY = 4096;
const char * const ERROR_MSG = "err ";
const char * const OK_MSG = "OK";

void write_all(int fd, const char * buf, size_t count) {
    if (!count) {
        count = strlen(buf) + 1;
    }
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

void my_close(int fd) {
    if (close(fd) == -1) {
        perror("close");
        _exit(EXIT_FAILURE);
    }
}

int main(int argc, char * argv[]) {
    if (argc != 2) {
        write_all(STDERR_FILENO, "Usage: client <filename>", 0);
    }

    struct addrinfo hints;
    struct addrinfo * result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;

    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */

    int s = getaddrinfo(NULL, SERVICE, &hints, &result);
    if (s) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        _exit(EXIT_FAILURE);
    }

    int sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sockfd == -1) {
        perror("socket");
        _exit(EXIT_FAILURE);
    }

    connect(sockfd, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);

    write_all(sockfd, argv[1], 0);

    int r;
    size_t len = 0;
    char * buf = malloc(CAPACITY);
    if (buf == NULL) {
        perror("malloc");
        free(buf);
        _exit(EXIT_FAILURE);
    }
    // firstly read OK or err
    while ((r = read(sockfd, buf + len, CAPACITY - len)) > 0) {
        size_t i;
        for (i = len; i != len + r; ++i) {
            if (buf[i] == '\0') {
                break;
            }
        }
        len += r;
    }
    if (r == -1) {
        perror("read");
        free(buf);
        _exit(EXIT_FAILURE);
    }

    if (strcmp(buf, OK_MSG)) {
        // ERROR
        len -= (strlen(ERROR_MSG) + 1);
        memmove(buf, buf + strlen(ERROR_MSG) + 1, len);

        // read error
        while ((r = read(sockfd, buf + len, CAPACITY - len)) > 0) {
            size_t i;
            for (i = len; i != len + r; ++i) {
                if (buf[i] == '\0') {
                    break;
                }
            }
            len += r;
        }
        if (r == -1) {
            perror("read");
            free(buf);
            _exit(EXIT_FAILURE);
        }

        write_all(STDERR_FILENO, buf, 0);
    } else {
        // OK
        len -= (strlen(OK_MSG) + 1);
        memmove(buf, buf + strlen(OK_MSG) + 1, len);

        // read size
        while ((r = read(sockfd, buf + len, CAPACITY - len)) > 0) {
            size_t i;
            for (i = len; i != len + r; ++i) {
                if (buf[i] == '\0') {
                    break;
                }
            }
            len += r;
        }
        if (r == -1) {
            perror("read");
            free(buf);
            _exit(EXIT_FAILURE);
        }

        size_t size = (size_t) atoi(buf);
        size_t l = strlen(buf);
        len -= (l + 1);
        memmove(buf, buf + l + 1, len);

        int eof = 0;
        while (!eof) {
            r = read(sockfd, buf + len, CAPACITY - len);
            if (r == -1) {
                perror("read");
                free(buf);
                _exit(EXIT_FAILURE);
            }
            eof = !r;
            len += r;
            int written = write(STDOUT_FILENO, buf, len);
            size -= written;
            if (written == -1) {
                perror("write");
                free(buf);
                _exit(EXIT_FAILURE);
            }
            len -= written;
            memmove(buf, buf + written, len);
        }
        size -= len;
 
        if (size != 0) {
            write_all(STDERR_FILENO, "Error occured. File was not fully received", 0);
        }

    }

    free(buf);
    my_close(sockfd);
    _exit(EXIT_SUCCESS);
}
