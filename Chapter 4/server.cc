// #include <stdio.h>
// #include <stdlib.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <unistd.h>
// #include <netinet/in.h>
// #include <string.h>
// #include <errno.h>
// #include <assert.h>

// static void msg(const char* msg) {
//     fprintf(stderr, "%s\n", msg);
// }

// static void die(const char* msg) {
//     int err = errno;
//     fprintf(stderr, "[%d] %s\n", err, msg);
//     abort();
// }

// const size_t k_max_msg = 4096;

// static int32_t read_full(int fd, char* buf, size_t bytes_to_read) {
//     while (bytes_to_read > 0) {
//         errno = 0;
//         ssize_t rv = read(fd, buf, bytes_to_read);
//         if (rv < 0) {
//             if (errno == EINTR) {
//                 continue; // interrupted by signal, retry
//             } else {
//                 return rv;
//             }
//         }
//         if (rv == 0) {
//             // EOF reached
//             return 0;
//         }
//         assert((size_t) rv <= bytes_to_read);
//         bytes_to_read -= rv;
//         buf += rv;
//     }
//     return 0;
// }

// static int32_t write_all(int fd, char* buf, size_t bytes_to_write) {
//     while (bytes_to_write > 0) {
//         ssize_t rv = write(fd, buf, bytes_to_write);
//         if (rv <= 0) {
//             return -1;
//         }
//         assert((size_t)rv <= bytes_to_write);
//         bytes_to_write -= rv;
//         buf += rv;
//     }

//     return 0;
// }

// static int32_t one_request(int conn_fd) {
//     // 4 bytes header
//     char rbuf[4 + k_max_msg];
//     errno = 0; // clear errno
//     int32_t err = read_full(conn_fd, rbuf, 4); // read the 4 bytes for payload size
//     if (err) {
//         msg(errno == 0 ? "EOF" : "read() error");
//         return err;
//     }

//     uint32_t len = 0;
//     memcpy(&len, rbuf, 4); // assume little-endian
    
//     // char len_buf[100];
//     // snprintf(len_buf, 10, "len: %d", len);
//     // msg(len_buf);

//     if (len > k_max_msg) {
//         msg("too long");
//         return -1;
//     }

//     // request body
//     err = read_full(conn_fd, &rbuf[4], len);
//     if (err) {
//         msg("read() error");
//         return err;
//     }

//     fprintf(stdout, "client says: %s\n", &rbuf[4]);

//     // reply using the same protocol
//     const char reply[] = "world";
//     char wbuf[4 + sizeof reply];
//     len = (uint32_t)strlen(reply);
//     memcpy(wbuf, &len, 4);
//     memcpy(&wbuf[4], reply, len);
//     return write_all(conn_fd, wbuf, 4 + len);
// }

// int main() {
//     int fd = socket(AF_INET, SOCK_STREAM, 0);
//     if (fd < 0) {
//         die("socket()");
//     }

//     int val = 1;
//     setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof val);

//     struct sockaddr_in addr = {};
//     addr.sin_family = AF_INET;
//     addr.sin_addr.s_addr = htonl(0); // wildcard addr 0.0.0.0
//     addr.sin_port = htons(1234);
//     int rv = bind(fd, (const struct sockaddr *) &addr, sizeof addr);
//     if (rv) {
//         die("bind()");
//     }

//     rv = listen(fd, SOMAXCONN);
//     if (rv) {
//         die("listen()");
//     }

//     while (true) {
//         struct sockaddr_in client_addr = {};
//         socklen_t addrlen = sizeof client_addr;
//         int conn_fd = accept(fd, (struct sockaddr *) &client_addr, &addrlen);
//         if (conn_fd < 0) {
//             continue; // some error
//         }

//         while (true) {
//             // here the server serves multiple requests by the same client
//             int32_t err = one_request(conn_fd);
//             if (err) {
//                 msg("process error");
//                 break;
//             }
//         }

//         close(conn_fd);
//     }
    
//     return 0;
// }

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>


static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

const size_t k_max_msg = 4096;

static int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error, or unexpected EOF
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t one_request(int connfd) {
    // 4 bytes header
    char rbuf[4 + k_max_msg];
    errno = 0;
    int32_t err = read_full(connfd, rbuf, 4);
    if (err) {
        msg(errno == 0 ? "EOF" : "read() error");
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4);  // assume little endian
    if (len > k_max_msg) {
        msg("too long");
        return -1;
    }

    // request body
    err = read_full(connfd, &rbuf[4], len);
    if (err) {
        msg("read() error");
        return err;
    }

    // do something
    fprintf(stderr, "client says: %.*s\n", len, &rbuf[4]);

    // reply using the same protocol
    const char reply[] = "world";
    char wbuf[4 + sizeof(reply)];
    len = (uint32_t)strlen(reply);
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], reply, len);
    return write_all(connfd, wbuf, 4 + len);
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    // this is needed for most server applications
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // bind
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0);    // wildcard address 0.0.0.0
    int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
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
        socklen_t addrlen = sizeof(client_addr);
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
        if (connfd < 0) {
            continue;   // error
        }

        while (true) {
            // here the server only serves one client connection at once
            int32_t err = one_request(connfd);
            if (err) {
                break;
            }
        }
        close(connfd);
    }

    return 0;
}