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

# Chapter 4: Request-Response Protocol
The previous chapter only processed a single request for each connection. We can make the server processes multiple requests in a single connection with a loop.

How does the server know how many bytes to read from the byte stream? That is the primary function of an application protocol. It has 2 levels of structure:
- A high level structure to split the byte stream into messages.
- Parse the structure inside each message a.k.a deserialization.

We use a simple binary protocol for demonstration (not the real redis protocol). We have 4-byte little endian integer indicating the length of the payload followed by the variable length payload.

## Parse the protocol
- `read`/`write` return the number of bytes read/
written.
- The return value is `-1` on error.
- `read` also returns `0` after `EOF` (or connection).

We cannot rely on the `read`/`write` method directly to perform our operations since `read`/`write` can return ***less than the requested number of bytes even under normal conditions (no `EOF`, no error)***

Why? Because `read`/`write` are common API for operations on both disk/network. The problem arises due to ***push-based IO*** and ***pull-based IO***. While reading from a disk, the OS already knows the length of the data that's going to be read i.e. it's a pull-based IO - We're *pulling* data that's already on the disk. So unless there's a signal interrupt, we are guaranteed to read the entire chunk/bytes requested. But in network, we'll have a peer that's *pushing* data to the socket. When the peer pushes data, it's written to a buffer and the task of transfering it over the network is deferred to the OS. It may happen that we receive the header that says the payload length is 100 bytes, but the peer has only yet managed to push say 20 bytes. That's why we need to read in a loop to completely read the data that's being pushed.

As for `write`, as discussed before, `write` just appends data to the kernel-side buffer - The actual network transfer is deferred to the OS. When the buffer becomes full, the caller must wait for the buffer to drain before copying the remaining data. Now during this wait, the syscall may be interrupted by a signal, causing `write` to return with partially written data. That's why we need to `write` in a loop.

### ERRNO gotchas
`errno` hold the last error code of a syscall. If `read`/`write` succeed, it won't hold it's success code. It'll have value of the last failed syscall on the system. Hence, `errno` is always set to `0` before making a syscall.

Now, during `read`, it must wait if the buffer is empty. If `read` is interrupted by a signal during this wait, it'll return back with `0`, but the exit code will be `-1` (instead of 0) and `errno` will be `EINTR`. But this is not an error. The `read` just returned back with `0` read bytes because of the interrupt. But a poorly coded implementation of `read_full` may treat this as err and return.

*The course leaves this as an exercise for the user to handle this case on their own.*

### A little about C things
- In both `write_all`/`read_full`, the `buf` pointer is `+=` with `rv`. What is essentially does is since we're first reading the 4 bytes at the start, in case we are not able to completely read the 4 bytes (due to conditions discussed above), we'll need to *seek* the pointer till the point where we've actually finished reading and then resume reading from there.

- Why do we read/write from buf[4] later? Since it's a pointer, isn't it already where we need it to be? **No**. Because in the `read_full`/`write_all` methods, it passed as a paramter and is now local to the method itself.

- When would the above case happen that the pointer is shifted globally? If a pointer to the pointer is passed to the method, it'll retain its changed values. (using double asterisk - **).

- `sizeof` gives the total size in bytes.
- `strlen` gives the total length in count of chars.

# Chapter 5: Concurrent IO Models
## Thread based concurrency
The client can hold a connection for as long as it wants, and the connection can be used for any number of request-response pairs. So that makes the server locked to handle a single connection only. This can be resolved by multithreading where we create a separate thread to handle each new connection.

### Why threads fail?
Though valid, threads have 2 drawbacks:
1. Memory: Many threads many stacks, increasing memory usage since there's no control on how much memory it can use.
2. Overhead: In stateless apps like PHP which creates many short-lived connections, it adds to the overhead of latency and CPU usage.

Forking new processes is older, and much worse.

## Event based concurrency
Concurrent IO is possible without threads. In Linux TCP stack handles sending and receiving packets by placing the packets in per-socket kernel side buffer. Consider `read` syscall. All it does it copy data from kernel side buffer, and when the buffer is empty, it suspends the `read` thread until more data is ready.

Same for `write`, it merely copies data to the kernel-side buffer and when it's full, it suspends the `write` thread until the buffer is empty.

So the need for multithreading arises due to need to wait for each socket to be ready (to read or write). If there was a way to wait for multiple sockets simultaneously, and then `read`/`write` whichever is ready, we could do it in a single thread.

This involves 3 operating system mechanisms:

Readiness notification: 
1. Wait for multiple sockets, return when one or more are ready. “Ready” means the read buffer is not empty or the write buffer is not full.
2. Non-blocking read: Assuming the read buffer is not empty, consume from it.
3. Non-blocking write: Assuming the write buffer is not full, put some data into it.

## Blocking and Non-blocking IO
If the read buffer is not empty, both blocking and non-blocking reads will return the data immediately.

If the buffer *is* empty, the non-blocking read will return with `errno = EAGAIN`, while a blocking read will block and wait for more data. Non-blocking read can be called repeatedly to fully drain the buffer.

If the write buffer is empty, both blocking and non-blocking writes will immediately write data to the buffer and return.

If it is *not* empty, the non-blocking will return with `errno = EGAIN` while the blocking write will wait for more room. Non-blocking writes can be called repeatedly to fully fill the write buffer. If the data is larger than the available write buffer, a non-blocking write will do a partial write, while a blocking write may block.

sockets can also be put in non-blocking mode using the `O_NONBLOCK` flag set using the `fcntl()` syscall.

```c++
static void fd_set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);  // get the flags
    flags |= O_NONBLOCK;                // modify the flags
    fcntl(fd, F_SETFL, flags);          // set the flags
}
```

## Readiness API
We use the simplest `poll` to wait for readiness on Linux. It takes in an array of socket fds, and some other flags to indicate whether we want to read (`POLLIN`) or write (`POLLOUT`) or both (`POLLIN | POLLOUT`).

There are other APIs too:
- `select()`: similar to `poll` and works on both Windows and Linux. But only allows for 1024 fds, so should not be used.
- `epoll_wait()`: Only works on Linux. Unlike poll(), the fd list is not passed as an argument, but stored in the kernel. epoll_ctl() is used to add or modify the fd list. It’s more scalable than poll() because passing a huge number of fds is inefficient.

## Readiness API for files
Readiness API cannot be used for files. In case of sockets, for eg during `read`, the data is present in the kernel side buffer. The read is guaranteed to return back data.

In case of files, no such buffer exists on the kernel side so the readiness for a file is undefined. These APIs will always report a disk file as ready, but the IO will block. So file IO must be done outside the event loop, in a thread pool, which we’ll learn later.

On Linux, file IO within an event loop may be possible with io_uring, which is a unified interface for both file IO and socket IO. But io_uring is a very different API, so we will not pursue it.

## Summary of concurrent IO techniques
|Type|Method|API|Scalability|
|----|------|----|-----------|
|Socket|Thread per connection|`pthread`|Low|
|Socket|Process per connection|`fork()`|Low|
|Socket|Event loop|`poll()`, `epoll`|High|
|File|Thread pool|`pthread`||
|Any|Event loop|`io_uring`|High|

