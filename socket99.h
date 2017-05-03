#ifndef SOCKET99_H
#define SOCKET99_H

#define SOCKET99_VERSION_MAJOR 0
#define SOCKET99_VERSION_MINOR 2
#define SOCKET99_VERSION_PATCH 0

#include <stdint.h>
#include <stdbool.h>
#include <netdb.h>

/* Max number of socket options to allow in the config struct.
 * (The first option_id of 0 will be treated as end-of-options.) */
#define SOCKET99_MAX_SOCK_OPTS 4

/* An option ID, value, sizeof(value) tuple for setsockopt(2). */
typedef struct socket99_sockopt {
    int option_id;
    void *value;
    socklen_t value_len;
} socket99_sockopt;

/* Configuration for a socket. Not all of these fields need to
 * be set, and ones omitted from a C99-style "designated initializer"
 * struct literal will be zeroed out and replaced with defaults. */
typedef struct {
    /* Hostname and port, for TCP or UDP sockets. */
    char *host;
    int port;

    /* Path, for Unix domain socket. */
    char *path;

    /* IPv4 or IPv6 address; if neither is specified, let OS decide.
     * These fields should be used in place of 'host' above. */
    char *IPv4;
    char *IPv6;

    bool server;                /* Listen for incoming clients? */
    bool datagram;              /* UDP or datagram Unix domain? */
    bool nonblocking;           /* non-blocking operation? */

    int backlog_size;           /* set a custom backlog size */

    socket99_sockopt sockopts[SOCKET99_MAX_SOCK_OPTS];
} socket99_config;

enum socket99_status {
    /* Socket created. */
    SOCKET99_OK = 0,

    /* Failures from socket API functions; most also save errno. */
    SOCKET99_ERROR_GETADDRINFO = -1,
    SOCKET99_ERROR_SOCKET = -2,
    SOCKET99_ERROR_BIND = -3,
    SOCKET99_ERROR_LISTEN = -4,
    SOCKET99_ERROR_CONNECT = -5,
    SOCKET99_ERROR_FCNTL = -6,

    /* Failure from snprintf: name too long. */
    SOCKET99_ERROR_SNPRINTF = -7,

    /* Invalid combination of options in configuration. */
    SOCKET99_ERROR_CONFIGURATION = -8,

    /* Error in setsockopt(2). */
    SOCKET99_ERROR_SETSOCKOPT = -9,

    /* Other unknown error. */
    SOCKET99_ERROR_UNKNOWN = -10,
};

/* Result from calling socket99_open with a given socket99_config. */
typedef struct {
    /* Result code and errno value from failure (if any). */
    enum socket99_status status;

    /* File descriptor, set if status is SOCKET99_OK (success). */
    int fd;

    /* Error code from socket(2), bind(2), etc. */
    int saved_errno;

    /* Error code from getaddrinfo, only set if status is
     * SOCKET99_ERROR_GETADDRINFO. See: gai_strerror(3). */
    int getaddrinfo_error;
} socket99_result;

/* Attempt to open a socket, according to the configuration stored in
 * CFG. Returns whether the socket opened; further details will be
 * stored in RES. */
bool socket99_open(socket99_config *cfg, socket99_result *res);

/* Construct an error message in BUF, based on the status codes
 * in *RES. This has the same return value and general behavior
 * as snprintf -- if the return value is >= buf_size, the string
 * has been truncated, but will be '\0'-terminated. */
int socket99_snprintf(char *buf, size_t buf_size, socket99_result *res);

/* Print an error message based on the status contained in *RES. */
void socket99_fprintf(FILE *f, socket99_result *res);

/* Set "hints" in an addrinfo struct, to be passed to getaddrinfo. */
void socket99_set_hints(socket99_config *cfg, struct addrinfo *hints);

#endif
