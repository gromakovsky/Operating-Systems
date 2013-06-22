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

const char * const SERVICE = "8822";
const int BACKLOG = 32;
const size_t CAPACITY = 4096;
const size_t MAX_CLIENTS = 32;
const int POLL_TIMEOUT = -1;

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

size_t read_all(int fd, char * buf) {
    int r;
    size_t len = 0;
    while ((r = read(fd, buf, CAPACITY - len) > 0)) {
        len += r;
    }
    if (r == -1) {
        perror("read");
        _exit(EXIT_FAILURE);
    }
    return len;
}

void my_close(int fd) {
    if (close(fd) == -1) {
        perror("close");
        _exit(EXIT_FAILURE);
    }
}

struct node {
    node * left, * right, * parent;
    std::string value;

    node() : left(nullptr), right(nullptr), parent(nullptr) {
    }

    ~node() {
        delete left;
        delete right;
        delete this;
    }
};

node * parse_tree(std::string s) {
    node * result = new node();
    if (s[0] != '<') {
        return result;
    }
    size_t t = s.find('>');
    if (t == std::string::npos) {
        delete result;
        return nullptr;
    }
    result->left = parse_tree(std::string(s.begin() + 1, s.begin() + t));
    s.erase(0, t + 1);
    t = s.find('<');
    if (t == std::string::npos) {
        return result;
    }
    result->value = s.substr(0, t);
    s.erase(0, t);
    result->right = parse_tree(s);
    return result;
}

std::string tree_to_string(node * tree) {
    std::string result;
    if (tree->left == nullptr) {
        result += "<()>";
    } else {
        result += tree_to_string(tree->left);
    }
    result += tree->value;
    if (tree->right == nullptr) {
        result += "<()>";
    } else {
        result += tree_to_string(tree->right);
    }
    return result;
}

node * move(node * root, std::string s) {
    if (root == nullptr) {
        return nullptr;
    }
    for (auto it = s.begin(); *it != 'h'; ++it) {
        if (*it == 'l') {
            if (root->left == nullptr) {
                return nullptr;
            } else {
                root = root->left;
            }
            continue;
        }
        if (*it == 'r') {
            if (root->right == nullptr) {
                return nullptr;
            } else {
                root = root->right;
            }
            continue;
        }
        return nullptr;
    }
    return root;
}

/*int add(std::string s) {
    return 0;
}*/

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

        node * root = new node();

        pollfd fds[MAX_CLIENTS + 1];
        fds[0].fd = sockfd;
        fds[0].events = POLLIN;
        nfds_t nfds = 1;

        const short ERRORS = POLLERR | POLLHUP | POLLNVAL;

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
                    char buf[CAPACITY];
                    buf[CAPACITY - 1] = '\0';
                    size_t r = read_all(fds[i].fd, buf);
                    if (r > CAPACITY - 2) {
                        write_all(STDERR_FILENO, "Not enough CAPACITY", 0);
                        _exit(EXIT_FAILURE);
                    }
                    switch (buf[0]) {
                        case 'a': {
                            const char * command = "add";
                            buf[strlen(command)] = '\0';
                            if (!strcmp(buf, command)) {
                            } else {
                                write_all(fds[i].fd, "Invalid command", 0);
                            }
                            break;
                        }
                        case 'p': {
                            const char * command = "print";
                            buf[strlen(command)] = '\0';
                            if (!strcmp(buf, command)) {
                                node * tree = move(root, buf + strlen(command) + 1);
                                if (tree == nullptr) {
                                    write_all(fds[i].fd, "Error occured", 0);
                                } else {
                                    write_all(fds[i].fd, tree_to_string(tree).c_str(), 0);
                                }
                            } else {
                                write_all(fds[i].fd, "Invalid command", 0);
                            }
                            break;
                        }
                        case 'd': {
                            const char * command = "del";
                            buf[strlen(command)] = '\0';
                            if (!strcmp(buf, command)) {
                            } else {
                                write_all(fds[i].fd, "Invalid command", 0);
                            }
                            break;
                        }
                        default: {
                            write_all(fds[i].fd, "Invalid command", 0);
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

        delete root;
        my_close(sockfd);
    }
    _exit(EXIT_SUCCESS);
}
