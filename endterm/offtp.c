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
const int BACKLOG = 32;
const size_t CAPACITY = 4096;

size_t my_read(int fd, char * buf) {
    int r;
    size_t len = 0;
    while ((r = read(fd, buf + len, CAPACITY - len)) > 0) {
        size_t i;
        for (i = len; i != len + r; ++i) {
            if (buf[i] == '\0') {
                return i;
            }
        }
        len += r;
    }
    if (r == -1) {
        perror("read");
        _exit(EXIT_FAILURE);
    }
    return len;
}

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

void itoa(off_t number, char * buf) {
    sprintf(buf, "%d", (int) number); 
}

int main() {
    pid_t pid = fork();
    if (pid) {
        printf("Demon started\n");
        waitpid(pid, NULL, 0);
        printf("Demon finished\n");
    } else {
        pid_t sid = setsid();
        if (sid == (pid_t) -1) {
            perror("setsid");
            _exit(EXIT_FAILURE);
        }

        int log_fd = dup(STDOUT_FILENO);
        if (log_fd == -1) {
            perror("dup");
            _exit(EXIT_FAILURE);
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
        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))) {
            perror("setsockopt");
            _exit(EXIT_FAILURE);
        }
        if (bind(sockfd, result->ai_addr, result->ai_addrlen) == -1) {
            perror("bind");
            _exit(EXIT_FAILURE);
        }
        freeaddrinfo(result);

        if (listen(sockfd, BACKLOG)) {
            perror("listen");
            _exit(EXIT_FAILURE);
        }

        int cfd;
        while (1) {
            struct sockaddr peer_addr;
            socklen_t peer_addr_len = sizeof(struct sockaddr);
            cfd = accept(sockfd, &peer_addr, &peer_addr_len); 
            if (cfd == -1) {
                perror("accept");
                _exit(EXIT_FAILURE);
            }
//            write_all(log_fd, "Connected\n", 0);

            if (!fork()) {
                char filename[CAPACITY];
                my_read(cfd, filename);                
/*                write_all(log_fd, "Read ", 0);
                write_all(log_fd, filename, 0);
                write_all(log_fd, "\n", 0);*/
                int fd = open(filename, O_RDONLY);
                if (fd == -1) {
                    write_all(cfd, "err ", 0);
                    write_all(cfd, strerror(errno), 0);
                } else {
                    write_all(cfd, "OK", 0);

                    struct stat sb;
                    stat(filename, &sb);
                    off_t file_size = sb.st_size;
                    char size_str[CAPACITY];
                    itoa(file_size, size_str);
                    write_all(cfd, size_str, 0);

                    char * buf = malloc(CAPACITY);
                    if (buf == NULL) {
                        perror("malloc");
                        _exit(EXIT_FAILURE);
                    }
                    size_t len = 0;
                    int eof = 0;
                    while (!eof) {
                        int r = read(fd, buf + len, CAPACITY - len);
                        if (r == -1) {
                            perror("read");
                            my_close(cfd);
                            free(buf);
                            _exit(EXIT_FAILURE);
                        }
                        eof = !r;
                        len += r;
                        int written = write(cfd, buf, len);
                        if (written == -1) {
                            perror("write");
                            my_close(cfd);
                            free(buf);
                            _exit(EXIT_FAILURE);
                        }
                        len -= written;
                        memmove(buf, buf + written, len);
                    }
                    free(buf);
                    my_close(fd);
                }

                my_close(cfd);
                _exit(EXIT_SUCCESS);
            } else {
                my_close(cfd);
            }

        }
        my_close(sockfd);
    }
    _exit(EXIT_SUCCESS);
}
