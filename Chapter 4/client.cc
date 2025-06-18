#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void msg(const char* msg) {
    fprintf(stderr, "%s\n", msg);
}

static void die(const char* msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

static int32_t read_full(int fd, char* buf, int bytes_to_read) {
    while (true) {
        errno = 0;
        ssize_t rv = read(fd, buf, bytes_to_read);
        if (rv < 0) {
            if (errno == EINTR) {
                continue; // signal interrupt, retry
            } else {
                return -1;
            }
        } else if (rv == 0) {
            // EOF
            break;
        }
        bytes_to_read -= rv;
        buf += rv;
    }
    return 0;
}

static int32_t write_all(int fd, char* buf, int bytes_to_write) {
    while (bytes_to_write > 0) {
        ssize_t rv = write(fd, buf, bytes_to_write);
        if (rv <= 0) {
            return -1;
        }
        bytes_to_write -= rv;
        buf += rv;
    }
    return 0;
}

const size_t k_max_msg = 4096;

static int32_t query(int fd, const char* text) {
    uint32_t len = (uint32_t)strlen(text);
    if (len > k_max_msg) {
        return -1;
    }

    char wbuf[4 + len];
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], text, len);
    if (int32_t err = write_all(fd, wbuf, 4 + len)) {
        return err;
    }

    char rbuf[4 + k_max_msg];
    errno = 0;
    int err = read_full(fd, rbuf, 4);
    if (err) {
        msg(errno == 0 ? "EOF" : "read() error");
        return err;
    }

    memcpy(&len, rbuf, 4);
    if (len > k_max_msg) {
        msg("too long");
        return -1;
    }

    err = read_full(fd, &rbuf[4], len);
    if (err) {
        msg("read() error");
        return err;
    }

    fprintf(stdout, "server says: %s\n", &rbuf[4]);
    return 0;
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof val);

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(1234);
    
    int rv = connect(fd, (struct sockaddr *) &addr, sizeof addr);
    if (rv) {
        die("connect()");
    }

    // multiple requests
    uint8_t query_num = 1;
    while (query_num < 4) {
        char msg_buf[10];
        ssize_t rv = snprintf(msg_buf, 10, "Hello%d", query_num);
        if (rv == 0) {
            die("sprintf()");
        }
        int err = query(fd, msg_buf);
        if (err) {
            return err;
            // break;
        }
        query_num++;
    }

    close(fd);
    return 0;
}