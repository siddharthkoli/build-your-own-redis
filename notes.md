# Chapter 2: Socket Programming
## TCP
tcp -> stream of bytes
protocol assembles the bytes into packets

multiple apps use the same number. how does the computer know what app a packet belongs to? -> it uses ports. this is called `demultiplexing`. 
port number is `16-bit` number. computer uses the following 4-tuple to identify the flow of info: `(src_ip, src_port, dest_ip, dest_port)`

tcp provides a byte stream, but we cannot directly interpret this byte stream. so we can either add a message layer to tcp or add reliable ordering to udp. 
the former is easier.

## Sockets
a socket is a handle used to refer to a connection.
on linux handle is called a `file_descriptor` (fd) and it's local to a process. nothing to do with files nor does it describe anything.
`socket()` method allocates a socket fd which is used to create the connection. this handle should be closed when done to free up resources.

listening on a socket is telling the os that the app is ready to accept tcp connections. the connection is also represented as a socket.
so there are 2 types of sockets: *listening socket and connection socket*.
the os first returns a listening socket to an app. this listening socket is then used by the app to accept tcp connections. this connection is represented by a connection socket handle.

creating a listening socket requires atleast 3 api calls:
1. obtain socket handle via `socket()`
2. set listening ip:port via `bind()`
3. create a listening socket via `listen()`

then we can use `accept()` to wait for incoming tcp connections.
```c++
// pseudo code:

fd = socket()
bind(fd, address)
listen(fd)
while (true)
    conn_fd = accept(fd)
    do_something_with(conn_fd)
    close(conn_fd)
```
from the client side:
1. obtain handle using socket()
2. create conn socket using connect()

```c++
// pseudo code:
fd = socket()
connect(fd, address)
do_something_with(fd)
close(fd)
```

`socket()` creates a typeless socket; the type (connection or listening) is determined after the `connect()` or `listen()` call.

## read and write
tcp and udp share the same socket api, but different implementation. both have `send()` and `recv()` methods.
for message based udp: `send`/`recv` correspond to packets.
for byte-stream based tcp: `send`/`recv` correspond to appending to/consuming from the byte-stream.

# Chapter 3: TCP Server and Client
Everything is written in C. Some features of C++ can be optionally used for convenience like `vector`s and `string`s. But knowledge of dynmaic arrays is a must.

Opening and reading `man` pages is also crucial. Refer to `man` pages on the shell or [Beej’s Guide](https://beej.us/guide/bgnet/html/split/) for online `man` pages and necessary sys-imports.

## Creating a TCP Server
### Step 1: Obtain socket handle
`socket()` takes 3 `integer` args:
```c++
int fd = socket(int domain, int type, int protocol)
```
- `domain` - can be either `AF_INET` for IPV4 or `AF_INET` for IPV6
- `type` - `SOCK_STREAM` for TCP, `SOCK_DGRAM` for UDP
- `protocol` - not worth diving into - keep 0

`domain` and `type` can be combined for different types of socket.

### Step 2: Set socket options
`setsockopt()` is used to set the socket options. These options are necessary but not mandatory.
```c++
setsockopt(int fd, int level, int option, int* value, socklen_t value_length)
```
- `fd` - socket handle
- `level` - level should be set to `SOL_SOCKET`
- `option` - the option whose value has to be set. Refer man page for all options.
- `value` - the value that option needs to be set with. Usually boolean values so 0 or non-0.
- `socklen_t` - length of the value pointer.

### Step 3: Bind to an address
Use `htonl` (host to network long) for IPv4 address and `htons` (host to network short) for port number. These methods are use to reverse the byte order which is also called as "***byte swap***". Both are stored in `struct sockaddr_in`. 
Here host refers to CPU endian and Network refers to big-endinan. Modern CPUs use little-endian and Networks use big-endian (also called network byte order).

`long` in `htonl` is actually `uint32_t`.
On a little-endian CPU, `htonl` results in byte swap.
On big-endian CPU, it does nothing.

We use `bind()` to bind the IP:Port pair to the socket.

### Step 4: Listen
The socket is actually created after `listen()`. The OS will automatically handle TCP handshakes and put the established connections in a queue. The app can then retrieve the connections using `accept()`

`listen()` also takes in a queue size. `SOMAXCONN` is 4096 on linux.

### Step 5: Accept connections
Use `accept()` to accept connections. Returns a sock fd. The server runs in a loop that accepts and processes each client connection.

### Step 6: Read & Write
We can use `read/write` or `send/recv` to perform our processing. Only diff is that `send/recv` has optional flags that can be set.

## Create a TCP Client
Steps:
1. Create socket fd
2. Connect using `connect()`
3. Write/Read to/from buffer.
4. Close using `close()`

