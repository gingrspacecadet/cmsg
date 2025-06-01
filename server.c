#include "common.h"

static int listenfd;
static int clients[MAXCLIENTS];

static void cleanup(void) {
    if (listenfd >= 0) close(listenfd);
    for (int i = 0; i < MAXCLIENTS; i++) {
        if (clients[i] >= 0) close(clients[i]);
    }
}

static void handle_sigint(int signo) {
    (void)signo;
    cleanup();
    exit(EXIT_SUCCESS);
}

int main_server(const char *port_str) {
    struct addrinfo hints, *res, *rp;
    int yes = 1;
    fd_set allset, rset;
    int maxfd, nready;

    // Install SIGINT handler to clean up
    signal(SIGINT, handle_sigint);

    // Prepare address info
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, port_str, &hints, &res) != 0) {
        fatal("getaddrinfo");
    }

    // Create and bind
    for (rp = res; rp; rp = rp->ai_next) {
        listenfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listenfd < 0) continue;
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
        if (bind(listenfd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(listenfd);
    }
    if (!rp) fatal("bind");

    freeaddrinfo(res);
    if (listen(listenfd, 10) < 0) fatal("listen");

    // Initialize client array
    for (int i = 0; i < MAXCLIENTS; i++) clients[i] = -1;

    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    maxfd = listenfd;

    fprintf(stdout, "Server listening on port %s...\n", port_str);

    while (1) {
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready < 0) {
            if (errno == EINTR) continue;
            fatal("select");
        }

        // New connection
        if (FD_ISSET(listenfd, &rset)) {
            struct sockaddr_in cliaddr;
            socklen_t cli_len = sizeof cliaddr;
            int connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &cli_len);
            if (connfd < 0) {
                perror("accept");
            } else {
                // Add to clients[]
                int i;
                for (i = 0; i < MAXCLIENTS; i++) {
                    if (clients[i] < 0) {
                        clients[i] = connfd;
                        break;
                    }
                }
                if (i == MAXCLIENTS) {
                    fprintf(stderr, "Too many clients\n");
                    close(connfd);
                } else {
                    FD_SET(connfd, &allset);
                    if (connfd > maxfd) maxfd = connfd;
                    char addrbuf[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &cliaddr.sin_addr, addrbuf, sizeof addrbuf);
                    fprintf(stdout, "New client: %s:%d (fd=%d)\n",
                            addrbuf, ntohs(cliaddr.sin_port), connfd);
                }
            }
            if (--nready <= 0) continue;
        }

        // Check all clients for data
        for (int i = 0; i < MAXCLIENTS; i++) {
            int sockfd = clients[i];
            if (sockfd < 0) continue;
            if (FD_ISSET(sockfd, &rset)) {
                char buf[MAXLINE];
                ssize_t n = read(sockfd, buf, sizeof buf);
                if (n <= 0) {
                    // Client closed or error
                    if (n == 0) {
                        fprintf(stdout, "Client fd=%d disconnected\n", sockfd);
                    } else {
                        perror("read");
                    }
                    close(sockfd);
                    FD_CLR(sockfd, &allset);
                    clients[i] = -1;
                } else {
                    // Broadcast to all other clients
                    buf[n] = '\0';
                    for (int j = 0; j < MAXCLIENTS; j++) {
                        int outfd = clients[j];
                        if (outfd >= 0 && outfd != sockfd) {
                            if (write(outfd, buf, n) < 0) {
                                perror("write to client");
                            }
                        }
                    }
                }
                if (--nready <= 0) break;
            }
        }
    }

    cleanup();
    return 0;
}
