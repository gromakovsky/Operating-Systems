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
#include <string>
#include <iostream>
#include <unordered_map>

const char * const SERVICE = "8822";
const int BACKLOG = 32;
const size_t CAPACITY = 4096;
const size_t MAX_CLIENTS = 32;
const int POLL_TIMEOUT = -1;

char * my_malloc(size_t size) {
    char * res = (char *) malloc(size);
    if (res == NULL) {
        _exit(EXIT_FAILURE);
    }
    return res;
}

void my_close(int fd) {
    if (close(fd) == -1) {
        perror("close");
        _exit(EXIT_FAILURE);
    }
}

struct my_struct {
    char * buf;
    size_t len;
    std::string output;
    short state; // 0 -- don't know, 1 -- rock, 2 -- scissors, 3 -- paper

    my_struct() {
        buf = my_malloc(CAPACITY);
        len = 0;
        buf[0] = 0;
        state = 0;
    }

    ~my_struct() {
        free(buf);
    }
};

size_t enemy(size_t i) {
    return i % 2 == 0 ? i - 1 : i + 1;
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

        pollfd fds[MAX_CLIENTS + 1];
        fds[0].fd = sockfd;
        fds[0].events = POLLIN;
        nfds_t nfds = 1;

        my_struct inputs[MAX_CLIENTS + 1];

        const short ERRORS = POLLERR | POLLHUP | POLLNVAL | POLLRDHUP;

        const char rock = 'r';
        const char scissors = 's';
        const char paper = 'p';

        while (true) {
            int poll_res = poll(fds, nfds, POLL_TIMEOUT);
            if (poll_res == -1) {
                perror("poll");
                _exit(EXIT_FAILURE);
            }

            for (size_t i = 1; i != nfds; ++i) {
                if (fds[i].revents & ERRORS) {
//                    std::cout << "ERRORS: " << i << " " << inputs[i].name << std::endl;
/*                    fds[i].events = 0;
                    fds[i].fd = -1;
                    fds[enemy(i)].events = 0;
                    fds[enemy(i)].fd = -1;*/
                    for (size_t j = i + 1; j != nfds; ++j) {
                        fds[j - 1] = fds[j];
                        inputs[j - 1] = inputs[j];
                    }
                    for (size_t j = enemy(i) + 1; j != nfds; ++j) {
                        fds[j - 1] = fds[j];
                        inputs[j - 1] = inputs[j];
                    }
                    --nfds;
                    continue;
                }

                if (fds[i].revents & POLLIN) {
//                    std::cout << "POLLIN: " << i << " " << inputs[i].name << std::endl;
                    size_t j = enemy(i);
                    int r = read(fds[i].fd, inputs[i].buf + inputs[i].len, CAPACITY - inputs[i].len);
                    if (r == -1) {
                        perror("read");
                        _exit(EXIT_FAILURE);
                    }
                    inputs[i].len += r;
                    if (inputs[i].state) {
                        inputs[i].output += "Please wait\n";
                        fds[i].events |= POLLOUT;
                    } else {
                        if (inputs[i].buf[1] != '\n') {
                            fprintf(stderr, "Invalid operation\n");
                        }
                        switch (inputs[i].buf[0]) {
                            case rock : {
                                inputs[i].state = 1;
                                break;
                            }                
                            case scissors: {
                                inputs[i].state = 2;
                                break;
                            }                
                            case paper : {
                                inputs[i].state = 3;
                                break;
                            }
                            default : {
                                fprintf(stderr, "Invalid operation\n");
                            }
                        }
                        if (inputs[i].len > 2) {
                            inputs[i].output += "Please wait\n";
                            fds[i].events |= POLLOUT;
                        }
                    }
                    inputs[i].len = 0;
                    if (inputs[j].state) {
//                        std::cout << inputs[i].state << " " << inputs[j].state << std::endl;
                        std::string result = "Draw\n";
                        if ((inputs[i].state - inputs[j].state + 3) % 3 == 1) {
                            result = "Winner is first\n";
                        }
                        if ((inputs[i].state - inputs[j].state + 3) % 3 == 2) {
                            result = "Winner is second\n";
                        }
                        inputs[i].state = 0;
                        inputs[j].state = 0;
                        inputs[i].output += result;
                        inputs[j].output += result;
                        fds[i].events |= POLLOUT;
                        fds[j].events |= POLLOUT;
                    }
/*                    write(STDOUT_FILENO, "Buffer after read: ", strlen("Buffer after read: "));
                    write(STDOUT_FILENO, inputs[i].buf, inputs[i].len);
                    write(STDOUT_FILENO, "\n", 1);
                    printf("Buffer length: %d\n", inputs[i].len);*/
               }

                if (fds[i].revents & POLLOUT) {
                    if (!inputs[i].output.empty()) {
                        int w = write(fds[i].fd, inputs[i].output.c_str(), inputs[i].output.length());
                        if (w == -1) {
                            perror("write");
                            for (size_t j = i + 1; j != nfds; ++j) {
                                fds[j - 1] = fds[j];
                                inputs[j - 1] = inputs[j];
                            }
                            for (size_t j = enemy(i) + 1; j != nfds; ++j) {
                                fds[j - 1] = fds[j];
                                inputs[j - 1] = inputs[j];
                            }
                            --nfds;
                        } else {
                            inputs[i].output.erase(0, w);
    //                        std::cout << "POLLOUT: " << i << " " << name << std::endl;
    //                        std::cout << "Written: " << std::string(outputs[name].begin(), outputs[name].begin() + w) << std::endl;
    //                        std::cout << "Erased: " << w << std::endl;
                            if (inputs[i].output.empty()) {
                                fds[i].events = POLLIN | POLLRDHUP;
                            }
                        }
                    }
                }
            }

            if (fds[0].revents & POLLIN) {
                if (nfds < MAX_CLIENTS) {
                    struct sockaddr peer_addr;
                    socklen_t peer_addr_len = sizeof(struct sockaddr);
                    int new_client = accept(sockfd, &peer_addr, &peer_addr_len); 
                    fds[nfds].fd = new_client;
                    fds[nfds].events = POLLIN | POLLRDHUP;
                    ++nfds;
                }
            }
        }

        my_close(sockfd);
    }
    _exit(EXIT_SUCCESS);
}
