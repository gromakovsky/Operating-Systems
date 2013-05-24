#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

const char * SERVICE = "8822";
const int BACKLOG = 32;

int main() {
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
        if (!bind(sockfd, i->ai_addr, i->ai_addrlen)) {
            break;
        }
        close(sockfd);
    }
    freeaddrinfo(result);

    if (i == NULL) {
        perror("bind");
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
        pid_t pid = fork();
        if (!pid) {
            dup2(cfd, STDIN_FILENO);
            dup2(cfd, STDOUT_FILENO);
            dup2(cfd, STDERR_FILENO);
            write(STDOUT_FILENO, "hello\n", 6);
            close(sockfd);
            close(cfd);
            _exit(EXIT_SUCCESS);
        }
        break;
    }
    close(sockfd);
    close(cfd);
    _exit(EXIT_SUCCESS);
}
