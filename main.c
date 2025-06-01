#include "common.h"

// Forward declarations
int main_server(const char *port_str);
int main_client(const char *host, const char *port_str);

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s server <port>\n", argv[0]);
        fprintf(stderr, "  %s client <host> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "server") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s server <port>\n", argv[0]);
            return EXIT_FAILURE;
        }
        return main_server(argv[2]);
    }
    else if (strcmp(argv[1], "client") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: %s client <host> <port>\n", argv[0]);
            return EXIT_FAILURE;
        }
        return main_client(argv[2], argv[3]);
    }
    else {
        fprintf(stderr, "Unknown mode '%s'. Use 'server' or 'client'.\n", argv[1]);
        return EXIT_FAILURE;
    }
}
