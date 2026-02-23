#include "server.h"
#include <stdlib.h>

int main(int argc, char **argv) {
    int port = 8080;
    if (argc >= 2) port = atoi(argv[1]);
    return run_server(NULL, port);
}