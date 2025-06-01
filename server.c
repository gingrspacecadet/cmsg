#include "common.h"

typedef struct {
    int   fd;
    char  name[MAXNAME];    /* if name[0] == '\0', we haven’t read username yet */
    char  channel[MAXCHAN]; /* current channel for this client */
} client_t;

static int       listenfd = -1;
static client_t  clients[MAXCLIENTS];

/* Close all fds on exit */
static void cleanup(void) {
    if (listenfd >= 0) close(listenfd);
    for (int i = 0; i < MAXCLIENTS; i++) {
        if (clients[i].fd >= 0) close(clients[i].fd);
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
    int maxfd;

    signal(SIGINT, handle_sigint);

    /* 1) Set up listening socket */
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    if (getaddrinfo(NULL, port_str, &hints, &res) != 0) {
        fatal("getaddrinfo");
    }

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

    /* 2) Initialize client array */
    for (int i = 0; i < MAXCLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].name[0]    = '\0';
        clients[i].channel[0] = '\0';
    }

    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    maxfd = listenfd;

    fprintf(stdout, "Server listening on port %s...\n", port_str);

    while (1) {
        rset = allset;
        int nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready < 0) {
            if (errno == EINTR) continue;
            fatal("select");
        }

        /* --- New incoming connection? --- */
        if (FD_ISSET(listenfd, &rset)) {
            struct sockaddr_in cliaddr;
            socklen_t cli_len = sizeof cliaddr;
            int connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &cli_len);
            if (connfd < 0) {
                perror("accept");
            } else {
                /* Find a free client slot */
                int i;
                for (i = 0; i < MAXCLIENTS; i++) {
                    if (clients[i].fd < 0) {
                        clients[i].fd              = connfd;
                        clients[i].name[0]         = '\0';           /* username not known yet */
                        strncpy(clients[i].channel, "general", MAXCHAN);
                        clients[i].channel[MAXCHAN - 1] = '\0';
                        break;
                    }
                }
                if (i == MAXCLIENTS) {
                    fprintf(stderr, "Too many clients, rejecting fd=%d\n", connfd);
                    close(connfd);
                } else {
                    FD_SET(connfd, &allset);
                    if (connfd > maxfd) maxfd = connfd;
                    char addrbuf[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &cliaddr.sin_addr, addrbuf, sizeof addrbuf);
                    fprintf(stdout,
                            "New connection from %s:%d → assigned fd=%d (channel='general')\n",
                            addrbuf, ntohs(cliaddr.sin_port), connfd);
                    /* Next read from this fd is expected to be the username line. */
                }
            }
            if (--nready <= 0) continue;
        }

        /* --- Check existing clients for data --- */
        for (int i = 0; i < MAXCLIENTS; i++) {
            int sockfd = clients[i].fd;
            if (sockfd < 0) continue;
            if (!FD_ISSET(sockfd, &rset)) continue;

            char buf[MAXLINE];
            ssize_t n = read(sockfd, buf, MAXLINE - 1);
            if (n <= 0) {
                /* Client disconnected or error */
                if (n == 0) {
                    if (clients[i].name[0] != '\0') {
                        /* Announce “<username> left” to everyone in the same channel */
                        char leave_msg[MAXLINE];
                        snprintf(leave_msg, sizeof leave_msg,
                                 "*** %s has left %s ***\n",
                                 clients[i].name, clients[i].channel);
                        for (int j = 0; j < MAXCLIENTS; j++) {
                            if (clients[j].fd >= 0
                                && j != i
                                && strcmp(clients[j].channel, clients[i].channel) == 0)
                            {
                                write(clients[j].fd, leave_msg, strlen(leave_msg));
                            }
                        }
                        fprintf(stdout, "Client '%s' (fd=%d) disconnected from channel '%s'\n",
                                clients[i].name, sockfd, clients[i].channel);
                    } else {
                        fprintf(stdout,
                                "Unnamed client (fd=%d) disconnected before setting username\n",
                                sockfd);
                    }
                } else {
                    perror("read");
                }
                close(sockfd);
                FD_CLR(sockfd, &allset);
                clients[i].fd = -1;
                clients[i].name[0]    = '\0';
                clients[i].channel[0] = '\0';
            } else {
                buf[n] = '\0';
                /* Trim trailing newline if present */
                if (buf[n - 1] == '\n') {
                    buf[n - 1] = '\0';
                    n--;
                }

                /* 1) If this client has no name yet, treat this line as the username */
                if (clients[i].name[0] == '\0') {
                    /* Save username (up to MAXNAME-1 chars) */
                    strncpy(clients[i].name, buf, MAXNAME - 1);
                    clients[i].name[MAXNAME - 1] = '\0';
                    /* Announce join to everyone already in “general” (since that’s the only channel) */
                    char join_msg[MAXLINE];
                    snprintf(join_msg, sizeof join_msg,
                             "*** %s has joined %s ***\n",
                             clients[i].name, clients[i].channel);
                    for (int j = 0; j < MAXCLIENTS; j++) {
                        if (clients[j].fd >= 0
                            && j != i
                            && strcmp(clients[j].channel, clients[i].channel) == 0)
                        {
                            write(clients[j].fd, join_msg, strlen(join_msg));
                        }
                    }
                    fprintf(stdout, "Client fd=%d is now known as '%s' in channel '%s'\n",
                            sockfd, clients[i].name, clients[i].channel);
                }
                /* 2) Otherwise, parse this line as “<channel>:<message>” */
                else {
                    /* Expect exactly one colon separating channel name from message */
                    char *colon = strchr(buf, ':');
                    if (!colon) {
                        /* Malformed—ignore silently or send error back to client?
                           For simplicity, we just ignore it. */
                    } else {
                        size_t clen = colon - buf;
                        if (clen >= MAXCHAN) clen = MAXCHAN - 1;
                        char   chanbuf[MAXCHAN];
                        strncpy(chanbuf, buf, clen);
                        chanbuf[clen] = '\0';

                        /* Only broadcast if the channel matches the sender’s stored channel */
                        if (strcmp(chanbuf, clients[i].channel) == 0) {
                            /* Build “username: message\n” for broadcast */
                            const char *msgbody = colon + 1;
                            char outbuf[MAXLINE];
                            int  m = snprintf(
                                outbuf, sizeof outbuf,
                                "%s: %s\n", clients[i].name, msgbody
                            );
                            if (m < 0) m = 0;
                            if (m >= (int)sizeof outbuf) m = (int)sizeof outbuf - 1;

                            /* Send to every other client whose channel == chanbuf */
                            for (int j = 0; j < MAXCLIENTS; j++) {
                                if (clients[j].fd >= 0
                                    && j != i
                                    && strcmp(clients[j].channel, chanbuf) == 0)
                                {
                                    write(clients[j].fd, outbuf, m);
                                }
                            }
                            fprintf(stdout, "[%s@%s] %s\n",
                                    clients[i].name, chanbuf, msgbody);
                        }
                        /* else: client sent a different‐channel header—ignore it */
                    }
                }
            }

            if (--nready <= 0) break;
        }
    }

    cleanup();
    return 0;
}
