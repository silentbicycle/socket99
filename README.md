A wrapper library for the BSD sockets API.

# Why?

This library trades the series of `getaddrinfo`, `socket`, `connect`,
`bind`, `listen`, etc. functions and their convoluted, casted arguments
for just one function that takes two structs (configuration and output).
By [creatively using C99's "designated initializers"][1], the configuration
struct works rather like a configuration key/value hash; the output
struct contains either the socket file descriptor or error information.

The sheer generality of the BSD sockets API also makes it rather
unwieldy. While the sockets API can be used for a lot of esoteric
things, there's no reason common use cases such as opening a TCP socket
to a given host and port should take dozens of lines of code.

[1]: https://spin.atomicobject.com/2014/10/08/c99-api-designated-initializer/


# License

socket99 is released under the ISC license.


# Requirements

This depends on C99 and a POSIX environment. You've got one of those
lying around somewhere, right?


# Basic Usage

Look at the fields in `struct socket99_config` listen in `socket99.h`,
call `socket99_open` with a pointer to a configuration struct using the
C99 designated initializer syntax. Only a few of the fields will be
used, such as:

    socket99_config cfg = {
        .host = "127.0.0.1",
        .port = 8080,
        .server = true,
        .nonblocking = true,
    };

for a non-blocking TCP server that listens to 127.0.0.1. This function
will return a bool for whether the socket was successfully created, and
the result struct argument will be modified to contain a status code and
either a file descriptor (on success) or error information on failure:

    socket99_result res;   // result output in this struct
    bool ok = socket99_open(&cfg, &res);

The configuration and result structs are no longer needed after the
result struct's file descriptor has been saved / errors are handled, so
both structs can be stack-allocated.

For more usage examples, look at `test_socket99.c`.


# Running the tests

To run the tests:

    $ make test
    
Note that the tests create a couple short-lived sockets on port 8080,
and if that port is already in use, the tests will fail. (This may be
configurable in a future release; it should probably check an
environment variable but default to 8080.)


# Supported Use Cases

+ Client and server

+ TCP, UDP, and Unix domain sockets (either stream and datagram)

+ Blocking and nonblocking

+ IPV4, IPv6, and "don't care"

+ setsockopt(2) options


# Future Development

Capturing other common use cases for sockets would be good.
