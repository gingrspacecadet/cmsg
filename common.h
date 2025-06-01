#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define MAXLINE    512
#define MAXCLIENTS FD_SETSIZE
#define MAXNAME    32
#define MAXCHAN    32

static void fatal(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

#endif /* COMMON_H */
