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
    r = read(fd, buf, CAPACITY - len);
    if (buf[r - 1] == '\n') {
        buf[r - 1] = '\0';
    }
    return r;
    while (true) {
        r = read(fd, buf, CAPACITY - len);
        if (r == -1) {
            perror("read");
            _exit(EXIT_FAILURE);
        }
        if (r == 0) {
            break;
        }
        len += r;
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
    node * left, * right;
    std::string value;

    node() : left(nullptr), right(nullptr) {
    }

    ~node() {
        delete left;
        delete right;
    }
};

node * parse_tree(std::string s) {
    node * result = new node();
    if (s[0] != '<') {
        return nullptr;
    }
    std::string l;
    size_t i = 1;
    size_t k = 1;
    while (i < s.length() && k != 0) {
        if (s[i] == '<') {
            ++k;
        }
        if (s[i] == '>') {
            --k;
        }
        if (k == 0) {
            result->left = parse_tree(l);
            ++i;
            break;
        }
        l += s[i];
        ++i;
    }
    if (i == s.length()) {
        return nullptr;
    }

    std::string value;
    while (i < s.length() && s[i] != '<') {
        value += s[i];
        ++i;
    }
    if (i == s.length()) {
        return nullptr;
    }
    result->value = value;

    std::string r;
    k = 1;
    ++i;
    while (i < s.length() && k != 0) {
        if (s[i] == '<') {
            ++k;
        }
        if (s[i] == '>') {
            --k;
        }
        if (k == 0) {
            result->right = parse_tree(r);
            break;
        }
        r += s[i];
        ++i;
    }
    if (i == s.length() && s.back() != ')') {
        return nullptr;
    }

    return result;
}

std::string tree_to_string(node * tree) {
    std::string result = "<";
    if (tree->left == nullptr) {
        result += "()";
    } else {
        result += tree_to_string(tree->left);
    }
    result += ">";
    result += tree->value;
    result += "<";
    if (tree->right == nullptr) {
        result += "()";
    } else {
        result += tree_to_string(tree->right);
    }
    result += ">";
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

node * move_to_parent(node * root, std::string s) {
    if (root == nullptr) {
        return nullptr;
    }
    node * parent = nullptr;
    for (auto it = s.begin(); *it != 'h'; ++it) {
        if (*it == 'l') {
            if (root->left == nullptr) {
                return nullptr;
            } else {
                parent = root;
                root = root->left;
            }
            continue;
        }
        if (*it == 'r') {
            if (root->right == nullptr) {
                return nullptr;
            } else {
                parent = root;
                root = root->right;
            }
            continue;
        }
        return nullptr;
    }
    return parent;
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

        node * root = new node();
/*        root->value = "X";
        root->right = new node();
        root->right->value = "Y";*/

        pollfd fds[MAX_CLIENTS + 1];
        fds[0].fd = sockfd;
        fds[0].events = POLLIN;
        nfds_t nfds = 1;

        const short ERRORS = POLLERR | POLLHUP | POLLNVAL;

        while (true) {
//            write_all(STDOUT_FILENO, "Poll\n", 0);
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
                    size_t r = read_all(fds[i].fd, buf);
                    if (r > CAPACITY - 2) {
                        write_all(STDERR_FILENO, "Not enough CAPACITY", 0);
                        _exit(EXIT_FAILURE);
                    }
                    buf[r] = '\0';
                    switch (buf[0]) {
                        case 'a': {
                            const char * command = "add";
                            buf[strlen(command)] = '\0';
                            if (!strcmp(buf, command)) {
                                std::string s(buf + strlen(command) + 1);
                                size_t t = s.find(' ');
                                if (t == std::string::npos) {
                                    write_all(fds[i].fd, "Invalid arguments\n", 0);
                                    break;
                                }
                                std::string where = s.substr(0, t);
                                if (where.back() != 'h') {
                                    write_all(fds[i].fd, "Invalid arguments\n", 0);
                                    break;
                                }
                                node * p = move_to_parent(root, where);
                                s.erase(0, t + 1);
                                node * tree = parse_tree(s);
                                if (tree == nullptr) {
                                    write_all(fds[i].fd, "Can't parse tree\n", 0);
                                    break;
                                }
                                if (where == "h") {
                                    root = tree;
                                    write_all(fds[i].fd, "Added\n", 0);
                                    break;
                                }
                                if (p == nullptr) {
                                    write_all(fds[i].fd, "This node doesn't exist\n", 0);
                                    delete tree;
                                    break;
                                }
                                if (where[where.length() - 2] == 'l') {
                                    p->left = parse_tree(s);
                                    write_all(fds[i].fd, "Added\n", 0);
                                    break;
                                }
                                if (where[where.length() - 2] == 'r') {
                                    p->right = parse_tree(s);
                                    write_all(fds[i].fd, "Added\n", 0);
                                    break;
                                }
                                delete tree;
                                write_all(fds[i].fd, "Invalid arguments\n", 0);
                            } else {
                                write_all(fds[i].fd, "Invalid command\n", 0);
                            }
                            break;
                        }
                        case 'p': {
                            const char * command = "print";
                            buf[strlen(command)] = '\0';
                            if (!strcmp(buf, command)) {
                                std::string s(buf + strlen(command) + 1);
                                node * tree = move(root, buf + strlen(command) + 1);
                                if (tree == nullptr) {
                                    write_all(fds[i].fd, "This node doesn't exist", 0);
                                } else {
                                    write_all(fds[i].fd, tree_to_string(tree).c_str(), 0);
                                }
                            } else {
                                write_all(fds[i].fd, "Invalid command", 0);
                            }
                            write_all(fds[i].fd, "\n", 1);
                            break;
                        }
                        case 'd': {
                            const char * command = "del";
                            buf[strlen(command)] = '\0';
                            if (!strcmp(buf, command)) {
                                std::string s(buf + strlen(command) + 1);
                                if (s.back() != 'h') {
                                    write_all(fds[i].fd, "Invalid arguments\n", 0);
                                    break;
                                }
                                node * p = move_to_parent(root, s);
                                if (s == "h") {
                                    if (root->left) {
                                        delete root->left;
                                        root->left = nullptr;
                                    }
                                    if (root->right) {
                                        delete root->right;
                                        root->right = nullptr;
                                    }
                                    root->value = "";
                                    write_all(fds[i].fd, "Deleted\n", 0);
                                    break;
                                }
                                if (p == nullptr) {
                                    write_all(fds[i].fd, "This node doesn't exist\n", 0);
                                    break;
                                }
                                if (s[s.length() - 2] == 'l') {
                                    delete p->left;
                                    p->left = nullptr;
                                    write_all(fds[i].fd, "Deleted\n", 0);
                                    break;
                                }
                                if (s[s.length() - 2] == 'r') {
                                    delete p->right;
                                    p->right = nullptr;
                                    write_all(fds[i].fd, "Deleted\n", 0);
                                    break;
                                }
                                write_all(fds[i].fd, "Invalid arguments\n", 0);
                            } else {
                                write_all(fds[i].fd, "Invalid command\n", 0);
                            }
                            break;
                        }
                        default: {
                            write_all(fds[i].fd, "Invalid command\n", 0);
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
