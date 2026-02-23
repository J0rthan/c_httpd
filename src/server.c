#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int create_listen_fd(const char *host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int yes = 1;
    // 允许端口快速复用（避免重启时 bind 失败）
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);

    if (host == NULL || host[0] == '\0') {
        addr.sin_addr.s_addr = htonl(INADDR_ANY); // 0.0.0.0
    } else {
        if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
            close(fd);
            errno = EINVAL;
            return -1;
        }
    }

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, 128) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

// send 可能出现“短写”，必须循环直到全部发送完
static int send_all(int fd, const void *buf, size_t n) {
    const char *p = (const char *)buf;
    size_t left = n;

    while (left > 0) {
        ssize_t m = send(fd, p, left, 0);
        if (m < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (m == 0) return -1;
        p += (size_t)m;
        left -= (size_t)m;
    }
    return 0;
}

static void reply_text(int cfd, int code, const char *reason, const char *content_type, const char *body) {
    size_t blen = strlen(body);
    char header[512];

    int hlen = snprintf(header, sizeof(header),
                        "HTTP/1.1 %d %s\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %zu\r\n"
                        "Connection: close\r\n"
                        "\r\n",
                        code, reason, content_type, blen);

    if (hlen > 0) {
        (void)send_all(cfd, header, (size_t)hlen);
        (void)send_all(cfd, body, blen);
    }
}

static void handle_client(int cfd) {
    char req[4096];
    ssize_t r = recv(cfd, req, sizeof(req) - 1, 0);
    if (r <= 0) return;
    req[r] = '\0';

    // 只解析请求行：METHOD PATH VERSION
    char method[16], path[1024], version[16];
    if (sscanf(req, "%15s %1023s %15s", method, path, version) != 3) {
        reply_text(cfd, 400, "Bad Request", "text/plain; charset=utf-8", "Bad Request");
        return;
    }

    // 仅支持 GET
    if (strcmp(method, "GET") != 0) {
        reply_text(cfd, 405, "Method Not Allowed", "text/plain; charset=utf-8", "Method Not Allowed");
        return;
    }

    // v1：只实现 /
    if (strcmp(path, "/") == 0) {
        const char *body =
            "<!doctype html>"
            "<html><head><meta charset='utf-8'><title>C HTTP v1</title></head>"
            "<body><h1>Hello from C HTTP Server (v1)</h1>"
            "<p>Only supports GET / in v1.</p>"
            "</body></html>";
        reply_text(cfd, 200, "OK", "text/html; charset=utf-8", body);
        return;
    }

    // 其他路径 404
    reply_text(cfd, 404, "Not Found", "text/plain; charset=utf-8", "Not Found");
}

int run_server(const char *host, int port) {
    int lfd = create_listen_fd(host, port);
    if (lfd < 0) {
        perror("listen");
        return 1;
    }

    printf("Listening on %s:%d ...\n", (host && host[0]) ? host : "0.0.0.0", port);

    for (;;) {
        struct sockaddr_in cli;
        socklen_t clilen = sizeof(cli);

        int cfd = accept(lfd, (struct sockaddr *)&cli, &clilen);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        handle_client(cfd);
        close(cfd);
    }
}