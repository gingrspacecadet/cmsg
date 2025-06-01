#include "common.h"

int main_client(const char *host, const char *port_str) {
    struct addrinfo hints, *res, *rp;
    int sockfd;
    fd_set allset, rset;
    int maxfd;

    // Prepare address info
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        fatal("getaddrinfo");
    }

    // Connect to first valid
    for (rp = res; rp; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd < 0) continue;
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(sockfd);
    }
    if (!rp) fatal("connect");

    freeaddrinfo(res);
    fprintf(stdout, "Connected to %s:%s\n", host, port_str);

    FD_ZERO(&allset);
    FD_SET(STDIN_FILENO, &allset);
    FD_SET(sockfd, &allset);
    maxfd = (STDIN_FILENO > sockfd ? STDIN_FILENO : sockfd);

    while (1) {
        rset = allset;
        if (select(maxfd + 1, &rset, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            fatal("select");
        }

        // Data from server
        if (FD_ISSET(sockfd, &rset)) {
            char buf[MAXLINE];
            ssize_t n = read(sockfd, buf, sizeof buf - 1);
            if (n <= 0) {
                if (n == 0) {
                    fprintf(stdout, "Server closed connection\n");
                } else {
                    perror("read");
                }
                break;
            }
            buf[n] = '\0';
            printf("%s", buf);
            fflush(stdout);
        }

        // Data from stdin
        if (FD_ISSET(STDIN_FILENO, &rset)) {
            char buf[MAXLINE];
            if (fgets(buf, sizeof buf, stdin) == NULL) {
                // EOF on stdin: exit
                break;
            }
            size_t len = strlen(buf);
            if (write(sockfd, buf, len) != (ssize_t)len) {
                perror("write to server");
                break;
            }
        }
    }

    close(sockfd);
    return 0;
}
