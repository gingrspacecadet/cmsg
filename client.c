#include "common.h"

int main_client(const char *host, const char *port_str) {
    struct addrinfo hints, *res, *rp;
    int sockfd;
    fd_set allset, rset;
    int maxfd;

    /* 1) Resolve & connect */
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        fatal("getaddrinfo");
    }
    for (rp = res; rp; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd < 0) continue;
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(sockfd);
    }
    if (!rp) fatal("connect");
    freeaddrinfo(res);

    /* 2) Prompt for username */
    char namebuf[MAXNAME];
    fprintf(stdout, "Enter username (max %d chars): ", MAXNAME - 1);
    if (fgets(namebuf, sizeof namebuf, stdin) == NULL) {
        fprintf(stderr, "No username? Exiting.\n");
        close(sockfd);
        return EXIT_FAILURE;
    }
    size_t namelen = strlen(namebuf);
    if (namelen > 0 && namebuf[namelen - 1] == '\n') {
        namebuf[namelen - 1] = '\0';
        namelen--;
    }
    if (namelen == 0) {
        fprintf(stderr, "Empty username not allowed. Exiting.\n");
        close(sockfd);
        return EXIT_FAILURE;
    }
    /* Send the username as the very first line */
    if (write(sockfd, namebuf, namelen) != (ssize_t)namelen ||
        write(sockfd, "\n", 1) != 1) {
        perror("write username");
        close(sockfd);
        return EXIT_FAILURE;
    }

    fprintf(stdout, "Connected to %s:%s as '%s'.\n",
            host, port_str, namebuf);

    /* 3) Enter select() loop, watching both stdin and the server socket */
    FD_ZERO(&allset);
    FD_SET(STDIN_FILENO, &allset);
    FD_SET(sockfd,       &allset);
    maxfd = (STDIN_FILENO > sockfd ? STDIN_FILENO : sockfd);

    while (1) {
        rset = allset;
        if (select(maxfd + 1, &rset, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            fatal("select");
        }

        /* --- Data from server --- */
        if (FD_ISSET(sockfd, &rset)) {
            char buf[MAXLINE];
            ssize_t n = read(sockfd, buf, sizeof buf - 1);
            if (n <= 0) {
                if (n == 0) {
                    fprintf(stdout, "Server closed connection.\n");
                } else {
                    perror("read from server");
                }
                break;
            }
            buf[n] = '\0';
            fputs(buf, stdout);
            fflush(stdout);
        }

        /* --- Data from stdin --- */
        if (FD_ISSET(STDIN_FILENO, &rset)) {
            char buf[MAXLINE];
            if (fgets(buf, sizeof buf, stdin) == NULL) {
                /* EOF on stdin (Ctrl+D) => quit */
                break;
            }
            size_t len = strlen(buf);
            if (len > 0) {
                if (write(sockfd, buf, len) != (ssize_t)len) {
                    perror("write to server");
                    break;
                }
            }
        }
    }

    close(sockfd);
    return 0;
}
