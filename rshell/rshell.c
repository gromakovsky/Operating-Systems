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
            perror(gai_strerror(s));
            _exit(EXIT_FAILURE);
        }

        struct addrinfo * i;
        int sockfd;
        for (i = result; i != NULL; i = i->ai_next) {
            sockfd = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
            if (sockfd == -1) {
                continue;
            }
            int yes = 1;
            if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))) {
                continue;
            }
            if (bind(sockfd, i->ai_addr, i->ai_addrlen) == -1) {
                perror("bind");
                _exit(EXIT_FAILURE);
                break;
            }
            close(sockfd);
        }
        freeaddrinfo(result);

        if (i == NULL) {
            perror("setsockopt or socket");
            _exit(EXIT_FAILURE);
        }

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
            if (fork()) { //a
                int master, slave;
                char buf[CAPACITY];
                int fd = openpty(&master, &slave, buf, NULL, NULL);
                if (fd == -1) {
                    perror("opentty");
                    _exit(EXIT_FAILURE);
                }
                if (fork()) {
                    close(slave);
                    char buf[CAPACITY]; //from master to cfd
                    size_t len = 0;
                    char buf2[CAPACITY]; //from cfd to master 
                    size_t len2 = 0;
                    struct pollfd pollfds[2];
                    pollfds[0].fd = master;
                    pollfds[0].events = POLLIN | POLLOUT | POLLERR | POLLHUP;
                    pollfds[1].fd = cfd;
                    pollfds[1].events = POLLIN | POLLOUT | POLLERR | POLLHUP;
                    while (1) {
                        int poll_res = poll(pollfds, 2, -80);
                        if (poll_res == -1) {
                            if (errno == EINTR) {
                                continue;    
                            } else {
                                perror("poll");
                                _exit(EXIT_FAILURE);
                            }
                        }
                        if ((pollfds[0].revents & POLLIN) && (len < CAPACITY)) {
                            int r = read(master, buf + len, CAPACITY - len);    
                            if (r == -1) {
                                perror("read");
                                _exit(EXIT_FAILURE);
                            }
                        }
                        if ((pollfds[1].revents & POLLIN) && (len2 < CAPACITY)) {
                            int r = read(master, buf2 + len2, CAPACITY - len2);
                            if (r == -1) {
                                perror("read");
                                _exit(EXIT_FAILURE);
                            }
                        }
                        if ((pollfds[0].revents & POLLOUT) && (len > 0)) {
                            int w = write(cfd, buf, len);    
                            if (w == -1) {
                                perror("write");
                                _exit(EXIT_FAILURE);
                            }
                            len -= w;
                            memmove(buf, buf + w, len);
                        }
                        if ((pollfds[1].revents & POLLOUT) && (len2 > 0)) {
                            int w = write(cfd, buf2, len2);    
                            if (w == -1) {
                                perror("write");
                                _exit(EXIT_FAILURE);
                            }
                            len2 -= w;
                            memmove(buf2, buf2 + w, len2);
                        }
                    }
                    close(master);
                    close(cfd);
                    _exit(EXIT_FAILURE);
                } else {
                    close(master);
                    close(cfd);
                    setsid();
                    
                    int pty_fd = open(buf, O_RDWR);
                    if (pty_fd == -1) {
                        perror("open");
                        _exit(EXIT_FAILURE);
                    }
                    close(pty_fd);

                    dup2(slave, STDIN_FILENO);
                    dup2(slave, STDOUT_FILENO);
                    dup2(slave, STDERR_FILENO);

                    close(sockfd);
                    execlp("bash", "bash", NULL);
                    _exit(EXIT_FAILURE);
                }
            } else {
                close(cfd);
            }
        }
        close(sockfd);
    }
    _exit(EXIT_SUCCESS);
}
