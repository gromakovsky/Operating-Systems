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
#include <map>

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
    short state; // 0 -- unknown, 1 -- hello, 2 -- to_who, 3 -- msg
    std::string name;
    std::string to_who;
    bool endl;

    my_struct() {
        buf = my_malloc(CAPACITY);
        len = 0;
        buf[0] = 0;
        state = 0;
        endl = false;
    }

    ~my_struct() {
        free(buf);
    }
};

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
        std::map<std::string, size_t> map;
//        std::string outputs[MAX_CLIENTS + 1];
        std::map<std::string, std::string> outputs;

        const short ERRORS = POLLERR | POLLHUP | POLLNVAL;

        const char * HELLO = "Hello, my name is";
        const char * TO = "To";

        while (true) {
            int poll_res = poll(fds, nfds, POLL_TIMEOUT);
            if (poll_res == -1) {
                perror("poll");
                _exit(EXIT_FAILURE);
            }
            for (size_t i = 1; i != nfds; ++i) {
                if (fds[i].revents & ERRORS) {
                    fds[i].events = 0;
                    fds[i].fd = -1;
                    continue;
                }

                if (fds[i].revents & POLLIN) {
                    int r = read(fds[i].fd, inputs[i].buf + inputs[i].len, CAPACITY - inputs[i].len);
                    if (r == -1) {
                        perror("read");
                        _exit(EXIT_FAILURE);
                    }
                    inputs[i].len += r;
/*                    write(STDOUT_FILENO, "Buffer after read: ", strlen("Buffer after read: "));
                    write(STDOUT_FILENO, inputs[i].buf, inputs[i].len);
                    write(STDOUT_FILENO, "\n", 1);
                    printf("Buffer length: %d\n", inputs[i].len);*/
                    if (inputs[i].state == 0) {
                        if (!strncmp(inputs[i].buf, HELLO, strlen(HELLO))) {
                            memmove(inputs[i].buf, inputs[i].buf + strlen(HELLO), inputs[i].len - strlen(HELLO));
                            inputs[i].len -= strlen(HELLO);
                            inputs[i].state = 1;
                        }
                        if (!strncmp(inputs[i].buf, TO, strlen(TO))) {
                            memmove(inputs[i].buf, inputs[i].buf + strlen(TO), inputs[i].len - strlen(TO));
                            inputs[i].len -= strlen(TO);
                            inputs[i].state = 2;
                        }
                        if (inputs[i].len >= strlen(HELLO)) {
                            fprintf(stderr, "Invalid format\n");
                            _exit(EXIT_FAILURE);
                        }
                    }
                    if (inputs[i].state == 1) {
                        size_t j;
                        for (j = 0; j != inputs[i].len; ++j) {
                            if (inputs[i].buf[j] == '\n') {
                                inputs[i].buf[j] = '\0';
                                inputs[i].name = inputs[i].buf;
                                ++j;
                                memmove(inputs[i].buf, inputs[i].buf + j, inputs[i].len - j);
                                inputs[i].len -= j;
                                inputs[i].state = 0;
/*                                if (map.count(inputs[i].name)) {
                                    fprintf(stderr, "Name already exists\n");
                                }*/
                                map.insert(std::make_pair(inputs[i].name, i));
                                if (!outputs[inputs[i].name].empty()) {
                                    fds[i].events |= POLLOUT;
                                }
//                                printf("Added user: %s\n", inputs[i].name.c_str());
                                break;
                            }
                        }
                    }
                    if (inputs[i].state == 2) {
/*                        size_t j;
                        if (inputs[i].to_who.empty()) {
                            for (j = 0; j != inputs[i].len; ++j) {
                                if (inputs[i].buf[j] == '\n') {
                                    inputs[i].buf[j] = '\0';
                                    inputs[i].to_who = inputs[i].buf;
                                    ++j;
                                    memmove(inputs[i].buf, inputs[i].buf + j, inputs[i].len - j);
                                    inputs[i].len -= j;
                                    inputs[i].state = 3;
                                    if (map.count(inputs[i].to_who)) {
                                        fds[map[inputs[i].to_who]].events |= POLLOUT;
                                        outputs[map[inputs[i].to_who]] = "From" + inputs[i].name + ":\n";
                                    }
                                    break;
                                }
                            }
                        }*/
                        size_t j;
                        if (inputs[i].to_who.empty()) {
                            for (j = 0; j != inputs[i].len; ++j) {
                                if (inputs[i].buf[j] == '\n') {
                                    inputs[i].buf[j] = '\0';
                                    inputs[i].to_who = inputs[i].buf;
                                    ++j;
                                    memmove(inputs[i].buf, inputs[i].buf + j, inputs[i].len - j);
                                    inputs[i].len -= j;
                                    break;
                                }
                            }
                        }
                        if (map.count(inputs[i].to_who)) {
                            fds[map[inputs[i].to_who]].events |= POLLOUT;
//                            outputs[map[inputs[i].to_who]] = "From" + inputs[i].name + ":\n";
                            outputs[inputs[i].to_who] = "From" + inputs[i].name + ":\n";
                            inputs[i].state = 3;
                        }
                    }
                    if (inputs[i].state == 3) {
                        std::string index = inputs[i].to_who;
                        if (inputs[i].buf[0] == '\n' && inputs[i].endl) {
                            memmove(inputs[i].buf, inputs[i].buf + 1, inputs[i].len - 1);
                            inputs[i].len -= 1;
                            inputs[i].state = 0;
                        } else {
                            size_t j;
                            for (j = 0; (inputs[i].len > 0) && (j < inputs[i].len - 1); ++j) {
                                if (inputs[i].buf[j] == '\n' && inputs[i].buf[j + 1] == '\n') {
                                    ++j;
                                    std::string tmp(inputs[i].buf, inputs[i].buf + j);
                                    ++j;
                                    memmove(inputs[i].buf, inputs[i].buf + j, inputs[i].len - j);
                                    outputs[index] += tmp;
                                    inputs[i].len = 0;
                                }
                            }
                            if (j == inputs[i].len - 1) {
                                std::string tmp(inputs[i].buf, inputs[i].buf + inputs[i].len);
                                outputs[index] += tmp;
                                inputs[i].len = 0;
                            }
                        }
                        inputs[i].endl = (outputs[index].back() == '\n');
//                        printf("Output is: %s\n", outputs[index].c_str());
//                        printf("Endl: %d\n", inputs[i].endl);
                        if (!outputs[index].empty()) {
                            fds[map[index]].events |= POLLOUT;
                        }
                    }
                }

                if (fds[i].revents & POLLOUT) {
                    std::string name = inputs[i].name;
                    if (!outputs[name].empty()) {
//                        printf("POLLOUT\n");
                        int w = write(fds[i].fd, outputs[name].c_str(), outputs[name].size());
                        if (w == -1) {
                            perror("write");
                            _exit(EXIT_FAILURE);
                        }
                        outputs[name].erase(0, w);
                        if (outputs[name].empty()) {
                            fds[i].events = POLLIN;
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
                    fds[nfds].events = POLLIN;
                    ++nfds;
                }
            }
        }

        my_close(sockfd);
    }
    _exit(EXIT_SUCCESS);
}
