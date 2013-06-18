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

const char * const SERVICE = "8822";
const int BACKLOG = 32;
const size_t CAPACITY = 4096;
const int POLL_TIMEOUT = 30000;

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

void my_dup2(int oldfd, int newfd) {
    if (dup2(oldfd, newfd) == -1) {
        perror("dup2");
        _exit(EXIT_FAILURE);
    }
}

int main() {
    int pid = fork();
    if (pid) {
        waitpid(pid, NULL, 0);
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
            write_all(log_fd, "Connected\n", 0);
            if (!fork()) { //a
                int master, slave;
                char name[CAPACITY];
                if (openpty(&master, &slave, name, NULL, NULL) == -1) {
                    perror("openpty");
                    _exit(EXIT_FAILURE);
                }
                if (fork()) {
                    my_close(slave);

                    const nfds_t nfds = 2;
                    char * buf[nfds];
                    size_t len[nfds];
                    size_t i;
                    for (i = 0; i != nfds; ++i) {
                        buf[i] = malloc(CAPACITY);
                        len[i] = 0;
                    }

                    struct pollfd pollfd[nfds];
                    pollfd[0].fd = master;
                    pollfd[0].events = POLLIN;
                    pollfd[1].fd = cfd;
                    pollfd[1].events = POLLIN;

                    const short ERROR_EVENTS = POLLERR | POLLHUP | POLLNVAL;
                    int finished = 0;

                    write_all(cfd, "Rshell is working\n", 0);

                    while (!finished) {
                        int poll_res = poll(pollfd, nfds, POLL_TIMEOUT);
                        if (poll_res == -1) {
                            perror("poll");
                            _exit(EXIT_FAILURE);
                        }
                        if (poll_res == 0) {
                            write_all(cfd, "Time is out. Disconnecting\n", 0);
                            break;
                        }
                        for (i = 0; i != nfds; ++i) {
                            size_t j = 1 - i;
                            if (pollfd[i].revents & ERROR_EVENTS) {
                                write_all(cfd, "Error occured. Disconnecting\n", 0);
                                finished = 1;
                            }
                            if (pollfd[i].events & POLLIN) {
                                int r = read(pollfd[i].fd, buf[i] + len[i], CAPACITY - len[i]);
                                if (r == 0) {
                                    finished = 1;
                                }
                                if (r == -1) {
                                    perror("read");
                                    finished = 1;
                                }
                                len[i] += r;
                                pollfd[j].events |= POLLOUT;
                            }
                            if (pollfd[j].revents & POLLOUT) {
                                int w = write(pollfd[j].fd, buf[i], len[i]);
                                if (w == -1) {
                                    perror("write");
                                    finished = 1;
                                }
                                len[i] -= w;
                                if ((len[j] == 0) && (pollfd[j].events & POLLOUT)) {
                                    pollfd[i].events ^= POLLOUT;
                                }
                                memmove(buf[i], buf[i] + w, len[i]);
                            }
                        }
                    }


                    for (i = 0; i != nfds; ++i) {
                        free(buf[i]);
                    }

                    my_close(master);
                    my_close(cfd);
                    write_all(log_fd, "Disconnected\n", 0);
                    my_close(log_fd);
                    _exit(EXIT_SUCCESS);
                } else {
                    my_close(master);
                    my_close(cfd);
                    my_close(log_fd);
                    if (setsid() == (pid_t) -1) {
                        perror("setsid");
                        _exit(EXIT_FAILURE);
                    }
                    
                    int pty_fd = open(name, O_RDWR);
                    if (pty_fd == -1) {
                        perror(strcat("open ", name));
                        _exit(EXIT_FAILURE);
                    }
                    my_close(pty_fd);

                    my_dup2(slave, STDIN_FILENO);
                    my_dup2(slave, STDOUT_FILENO);
                    my_dup2(slave, STDERR_FILENO);
                    my_close(slave);
                    execl("/bin/bash", "bash", NULL);
                    perror("execl");
                    _exit(EXIT_FAILURE);
                }
            } else {
                my_close(cfd);
            }
        }
        my_close(sockfd);
    }
    _exit(EXIT_SUCCESS);
}
