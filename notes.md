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

# Chpater 6.1: Event Loop
## Per connection state
In an event loop, the processing for each connection's request or response may span multiple loop iterations. Hence we need some kind of state that persists across the iterations.

```c++
struct Conn {
    int fd = -1;
    // application's intention, for the event loop
    bool want_read = false;
    bool want_write = false;
    bool want_close = false;
    // buffered input and output
    std::vector<uint8_t> incoming;  // data to be parsed by the application
    std::vector<uint8_t> outgoing;  // responses generated by the application
};
```

- `Conn::want_read` & `Conn::want_write` tell the event loop that they're ready to read/write
- `Conn::want_close` tells the loop it's ready to close the connection.
- `Conn::incoming`: buffers the data from the socket for the protocol to work on.
- `Conn:outgoing`: buffers generated responses that are written to the socket.

The application code (server/client) communicate with the event loop using the `want_read`, `want_write` and `want_close` variables.

## Why buffers?
Since we're performing non-blocking reads, the entire read may not happen in a single iteration and could span across several iterations.
So, during each iteration, if the socket is ready to read:
1. Do a non-blocking read.
2. Add data to `Conn::incoming` buffer.
3. Try to parse the accumulated buffer.
    - If there's not enough data, do nothing.
4. Process the parsed message.
5. Drain the `Conn::incoming` buffer.

As for write, it's the same as above - non-blocking writes since writes span across several iterations for large messages.

## About polling
*Quoted from Beej's guide:*

> We’re going to ask the operating system to do all the dirty work for us, and just let us know when some data is ready to read on which sockets. In the meantime, our process can go to sleep, saving system resources.
>
> The general gameplan is to keep an array of `struct pollfds` with information about which socket descriptors we want to monitor, and what kind of events we want to monitor for. The OS will block on the `poll()` call until one of those events occurs (e.g. “socket ready to read!”) or until a user-specified timeout occurs.
>
> Usefully, a `listen()`ing socket will return “ready to read” when a new incoming connection is ready to be `accept()`ed.

> ```c++
> #include <poll.h>
> 
> int poll(struct pollfd fds[], nfds_t nfds, int > timeout);
> ```
>
> |Macro|Description|
> |-----|-----------|
> |`POLLIN`|Alert me when data is ready to recv() on this socket.|
> |`POLLOUT`|Alert me when I can send() data to this socket without blocking.|
> |`POLLHUP`|Alert me when the remote closed the connection.|
> 
> Once you have your array of `struct pollfds` in order, then you can pass it to `poll()`, also passing the size of the array, as well as a timeout value in milliseconds. (You can specify a negative timeout to wait forever.)
> 
> After `poll()` returns, you can check the revents field to see if `POLLIN` or `POLLOUT` is set, indicating that event occurred.

## The Event Loop code
We use a map called `fd2conn` indexed by `fd`.
Steps:
1. Construct the `fd` list for poll.
    - We put the listening socket `fd` always at the first position in the `fd` list for poll. That way, we always know which `fd` is the listening socket to accept new connections.
2. Call `poll()`
    - `poll()` is the only blocking syscall in the entire program. Normally it returns when at least one of the fds is ready. However, it may occasionally return with errno = EINTR even if nothing is ready.

    - If a process receives a Unix signal during a blocking syscall, the syscall is immediately returned with `EINTR` to give the process a chance to handle the signal. `EINTR` is not expected for non-blocking syscalls.

    - `EINTR` is not an error, the syscall should be 
    retried. 
3. Accept new connections.
   - `accept()` is treated as `read()` in readiness notifications, so it uses `POLLIN`. After `poll()` returns, check the 1st fd to see if we can `accept()`.
4. Invoke application callbacks
5. Terminate connections
    - We always `poll()` for `POLLERR` on connection sockets, so we can destroy the connection on error. Application code can also set `Conn::want_close` to request the event loop to destroy the connection.

## About the `buf_append` & `buf_consume` functions
```c++
// append to the back
static void
buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len) {
    buf.insert(buf.end(), data, data + len);
}
// remove from the front
static void buf_consume(std::vector<uint8_t> &buf, size_t n) {
    buf.erase(buf.begin(), buf.begin() + n);
}
```

We could've also used `buf.push_back()` but the function defined uses `insert` instead of `push_back`. Insert allows us to batch insert data into `buf` using the `len` arg provided to the function. This results in better performance as compared to using `push_back` in a loop. Both give the same results but batch_insert makes a single resizing of the vector whereas it may happen several times when using `push_back` in a loop.

Similar case in point for `buf_consume`.

# Chapter 6.2
A typical redis server can handle anywhere between 20K~200K requests/second for simiple ops like get, set, del. This number is limited by the number of IO ops a single thread can handle.

Since we're bound by how much work is done per IO call for our reqs/sec, it's desirable to do as much work as possible in each IO call. Hence we can batch IO reqs together (process multiple reqs/read).

This can be achieved using *pipelining*. Normally a server sends 1 req and receives 1 response. In pipelining, the client sends n reqs and receives n responses. The server still processes all the reqs in order, the only diff is that instead of reading 1 req in a single IO op, it now reads several reqs in the same single IO op, effectively reducing the no. of IO ops.

## Problem with pipelining
We cannot just let the server read a single req, process and send 1 resp because when the client sends several reqs, the server will read 1 req, send 1 response, and then wait for more reqs. This makes the pipeline get stuck.

The solution is instead of reading a single request, we keep reading from the input buffer until there is nothing left to do.
```c++
static void handle_read(Conn *conn) {
    // ...
    // try_one_request(conn);               // WRONG
    while (try_one_request(conn)) {}        // CORRECT
    // ...
}
```

And after we've processed 1 req, we cannot just empty the input buffer because there could be more reqs in the buffer i.e. we when waiting for n bytes to come, we cannot assume that *at most n bytes* have come. More bytes could've come due to several reqs.
```c++
static bool try_one_request(Conn *conn) {
    // ...
    // conn->incoming.clear()               // WRONG
    buf_consume(conn->incoming, 4 + len);   // CORRECT
    return true;
}
```

### Optimistic non-blocking writes
In normal req-resp, the client sends 1 req and waits for 1 resp. Normally, when the client sends a req, it has very likely consumed (or read) the previous response. So the write buffer is very likely empty.

So it's safe to assume from the server side that we can immediately write to the outgoing buffer (send a response) without `poll()`ing to see if the client is ready to read our response. (the `poll()` is most likely to succeed.) So we can save 1 syscall.

With pipelining, the client is sending several reqs. We can't be sure that when we're sending a response, the client is ready to read. Why? Because the client may be busy still sending reqs. So the above assumption here is `false`. We will get `EAGAIN` indicating that the requested resource is not available and we should retry.

**Note: Souce code for both 6.1 and 6.2 is same. So only 6.1 is saved**

# Chapter 7: Key Value Server
We'll add a basic KV store to the server and update the protocol to support it.

We'll use the following:

```c++
┌────┬───┬────┬───┬────┬───┬───┬────┐
│nstr│len│str1│len│str2│...│len│strn│
└────┴───┴────┴───┴────┴───┴───┴────┘
  4B   4B ...   4B ...

```

`nstr` is the number of items in the req.
Then each item is again `len` followed by `payload`.

The repsonse is just a simple status code and a string.
```c++
┌────────┬─────────┐
│ status │ data... │
└────────┴─────────┘
    4B     ...
```

## How to handle a request:
1. Parse the command
2. Process the command and generate the resp
3. Append the resp to the outgoing buffer.

We use a toy `map` from STL.

# Chapter 8.1 Hashtables
2 classes of data structures for KV store:
1. sorting based structures such as AVL tree, Trie, Treap, B-tree.
2. 2 types of hashtables: open addressing and chaining.

Sorting structures maintain order and use comparisons to narrow down their search having a time complexity of `O(log N)`.

Hashtables rely on uniformly distrubuted hash values and have a `O(1)` time complexity.

If we have unique integer keys, we can use an array to store the values like the `fd2Conn` structure used previously.
```c++
std::vector<Conn *> fd2conn;
```

If keys are not unique, then we can use nested structures:
```c++
std::vector<std::vector<T>> n;
```

Now if the keys are non-integers, we can reduce these arbitrary types to integer types using a hash function. We use these values obtained from the hash function to key the array.

Multiple non-integer values may hash to the same integer value -> this is called a collision. But for array-of-arrays type, we can just store both the key and value to distinguish between different keys.
```c++
std::vector<std::vector<pair<K, T>>> ,;
m.resize(N);
m[hash(key) % N].push_back(std::pair{key, val}); // insert


for (auto& [k, v]: m[hash(key) % N]) { // lookup
    if (k == key) { /* found */ }
}
```
This is a ***chaining*** type of hashtable, since we're chaining keys that hash to the same integer. The collection doesn't have to be arrays, array-of-linked-list is the most common one.
```c++
std::vector<std::vector<pair<K, V>>> m; // array of arrays
std::vector<std::list<pair<K, V>>> m; // array of linked list
```

***Open addressing***: Doesn't use nested collection - instead stores KV pairs directly into the array. In case of collision, it finds another slot and uses it if it's empty else it keeps on probling until it finds an empty slot.

## Why are hashtables `O(1)` on avg?
### Load factor
Let's say the hashtable has N slots and N slots can store k * N keys. Then every slot on avg stores k keys. This ratio is called ***`load_factor = keys/slots`***

Hashtables are `O(1)` on avg because there is a limit to the number of keys relative to the number of slots. When the maximum load_factor is reached, keys are migrated to a larger hashtable (this is called rehashing). This can be triggered by insertion. Like dynamic arrays, the new hashtable is exponentially larger, so the amortized insertion cost is still `O(1)`.

### What is amortized?
Essentially measure of values spread over time/operations. In this context, insertion is `O(1)`, but rehashing is `O(N)`. But measured over multiple insert operations, the avg complexity over all these operations still turns out to be `O(1)`.

## Why not use `STL` hashtables?
If we use `std::unordered_map` from STL over the network, there will be issues when it is deployed at scale.

There are 2 issues that arise when scale is involved:
1. Throughput
2. Latency

Throughput issues have generic, easy solutions such as sharding and read-only replicas. But latency issues are much harder to solve and are mostly domain specific.

The largest latency issue with hashtables comes from insertion, which may trigger an `O(N)` resize. A modern machine may easily have hundreds of GB of memory, but resizing a hashtable of this size will take a long time, rendering the service unavailable.

### Side note: what is throughput?
***Throughput*** is the total data passing through a measure at any point in time.

***Bandwidth*** is the max throughput that can be achieved in ideal conditions.

So: *Throughput <= Bandwidth*

and, *Throughput = Bandwidth * Efficiency*

## How to resolve this *resize* latency issue?
Resize the hashtable progressively. After allocating the new hashtable, don't move all the keys at once, only move a fixed number of keys. Then each time the hashtable is used, move some more keys. This can slow down lookups since we now have to query 2 hashtables.

Trick: After allocating a new hashtable, the values must be initialized (or zeroed out). So merely *triggering* a resize will seem to be `O(N)` time. But this can be avoided by also initializing the slots progressively. This can be done using `calloc()` instead of `malloc()`.

When we use `malloc()` it gives us memory from the heap which must be zeroed out (or initialized) since it may contain garbage data. But when we use `calloc()`, it gives just a virtual memory address for the application to use because `calloc()` will call `mmap()` to get it's memory. No real physical memory in the RAM has yet been allocated to that virtual address. Essentially, `mmap()` will give us pages that will then be zeroed out on first access, which effectively makes it a progressively zero-initialized array.

When `calloc()` is used for large memory allocations, it'll internally call `mmap()` to give virtual pages which will be initialized upon first access. This gives us a fast, `O(1)` response.

When `calloc()` is used for small memory allocations, it'll pull the memory directly from heap. This memory needs to be zeroed-out, which is a bit slower, but this latency is still bounded.

How do we know the threshold for what is small and large memory allocation? This is determined by the libc implementation.

## How to resolve the *chaining* latency issue?
Chaining hashtables results in the possiblity that a very long chain of collisions will exist. But even with this, chaining hashtables are still less prone to collisions as compared to open addressing because they still use the same slot when colliding. In open addressing, colliding keys will take multiple slots, which further increases the possibility of collisions.

We use *chaining hashtable with linked list* because:
1. Latency: Insertion and deletion are both `O(1)`.
2. Reference Stability: Pointers to data remain valid even after resizing.
3. Can be implemented as an intrusive data structure.

