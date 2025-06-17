#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> // for struct sockaddr_in
#include <errno.h>
#include <stdlib.h> // for abort()
#include <unistd.h> // for close(), read(), write()∑
#include <string.h> // for strlen()

static void msg(const char* msg) {
    fprintf(stderr, "%s\n", msg);
}

static void die(const char* msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

static void do_something(int conn_fd) {
    char rbuf[64] = {};
    ssize_t n = read(conn_fd, rbuf, sizeof(rbuf) - 1);
    if (n < 0) {
        msg("read() error");
        return;
    }
    fprintf(stdout, "client says: %s\n", rbuf);

    char wbuf[] = "world";
    write(conn_fd, wbuf, strlen(wbuf));
}

int main() {
    // create socket fd
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    // set socket options
    int value = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof value);

    // bind
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(0); // wildcard address 0.0.0.0
    addr.sin_port = htons(1234);
    int rv = bind(fd, (const struct sockaddr *) &addr, sizeof addr);
    if (rv) {
        die("bind()");
    }

    // listen
    rv = listen(fd, SOMAXCONN);
    if (rv) {
        die("listen()");
    }

    while (true) {
        // accept
        struct sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof client_addr;
        int conn_fd = accept(fd, (struct sockaddr *) &client_addr, &addrlen);
        if (conn_fd < 0) {
            continue; // error
        }

        do_something(conn_fd);
        close(conn_fd);
    }

    return 0;
}