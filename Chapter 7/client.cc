// stdlib
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
// system
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
// C++
#include <string>
#include <vector>
#include <map>

static void msg(const char* msg) {
    fprintf(stderr, "%s\n", msg);
}

static void msg_errno(const char* msg) {
    fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

static void die(const char* msg) {
    fprintf(stderr, "[%d] %s\n", errno, msg);
    abort();
}

static void fd_set_nb(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno) {
        die("fcntl get error");
        return;
    }

    flags |= O_NONBLOCK;
    errno = 0;
    (void) fcntl(fd, F_SETFL, flags);
    if (errno) {
        die("fcntl set error");
    }
}

const size_t k_max_msg = 32 << 20; // 32 * 2^20 (likely larger than kernel buffer)

struct Conn {
    int fd = -1;
    bool want_read = false;
    bool want_write = false;
    bool want_close = false;

    std::vector<uint8_t> incoming;
    std::vector<uint8_t> outgoing;
};

static void buf_append(std::vector<uint8_t>& buf, const uint8_t* data, size_t len) {
    buf.insert(buf.end(), data, data + len);
}

static void buf_consume(std::vector<uint8_t>& buf, size_t n) {
    buf.erase(buf.begin(), buf.begin() + n);
}

static Conn* handle_accept(int fd) {
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof client_addr;
    int conn_fd = accept(fd, (struct sockaddr*) &client_addr, &addrlen);
    if (conn_fd < 0) {
        msg_errno("accept() error");
        return NULL;
    }

    uint32_t ip = client_addr.sin_addr.s_addr;
    fprintf(
        stderr,
        "new client from %u.%u.%u.%u:%u\n",
        ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, ip >> 24,
        htonl(client_addr.sin_port)
    );

    fd_set_nb(conn_fd);

    Conn* conn = new Conn();
    conn->fd = conn_fd;
    conn->want_read = true;
    return conn;
}

const size_t k_max_args = 200 * 1000;

static bool read_u32(const uint8_t** cur, const uint8_t* end, uint32_t& out) {
    if (cur + 4 > end) {
        return false;
    }
    memcpy(&out, cur, 4);
    cur += 4;
    return true;
}

static bool read_str(const uint8_t** cur, const uint8_t* end, size_t n, )