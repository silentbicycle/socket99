/* 
 * Copyright (c) 2014 Scott Vokes <vokes.s@gmail.com>
 *  
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *  
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "socket99.h"

/* Built-in default backlog size. */
#define DEF_BACKLOG_SIZE SOMAXCONN   // very backlog. wow.

static bool set_defaults_and_check_cfg(socket99_config *cfg);
static bool make_tcp_udp(socket99_config *cfg, socket99_result *out);
static bool make_unixdomain(socket99_config *cfg, socket99_result *out);
static bool set_nonblocking(socket99_result *out);
static bool set_socket_options(socket99_config *cfg,
    socket99_result *out, int fd);
static const char *status_key(enum socket99_status s);

/* Attempt to open a socket, according to the configuration stored in
 * CFG. Returns whether the the socket opened; further details will be
 * stored in RES. */
bool socket99_open(socket99_config *cfg, socket99_result *res) {
    if (cfg == NULL || res == NULL) { return false; }
    memset(res, 0, sizeof(*res));

    if (!set_defaults_and_check_cfg(cfg)) {
        res->status = SOCKET99_ERROR_CONFIGURATION;
        return false;
    }

    if (cfg->path) {
        if (!make_unixdomain(cfg, res)) { return false; }
    } else {
        if (!make_tcp_udp(cfg, res)) { return false; }
    }

    if (cfg->nonblocking) {
        if (!set_nonblocking(res)) { return false; }
    }

    return true;
}

/* Construct an error message in BUF, based on the status codes
 * in *RES. This has the same return value and general behavior
 * as snprintf -- if the return value is >= buf_size, the string
 * has been truncated. Returns -1 if either BUF or RES are NULL. */
int socket99_snprintf(char *buf, size_t buf_size, socket99_result *res) {
    if (buf == NULL || res == NULL) { return 0; }
    return snprintf(buf, buf_size, "%s: %s",
        status_key(res->status),
        (res->status == SOCKET99_ERROR_GETADDRINFO
            ? gai_strerror(res->getaddrinfo_error)
            : strerror(res->saved_errno)));
}

/* Print an error message based on the status contained in *RES. */
void socket99_fprintf(FILE *f, socket99_result *res) {
    if (f == NULL || res == NULL) { return; }
    fprintf(f, "%s: %s\n", status_key(res->status),
        (res->status == SOCKET99_ERROR_GETADDRINFO
            ? gai_strerror(res->getaddrinfo_error)
            : strerror(res->saved_errno)));
}

/* Set "hints" in an addrinfo struct, to be passed to getaddrinfo. */
void socket99_set_hints(socket99_config *cfg, struct addrinfo *hints) {
    if (cfg == NULL || hints == NULL) { return; }
    memset(hints, 0, sizeof(*hints));

    /* if .IPv4 or .IPv6 are used, set and use that instead of *host */
    if (cfg->path) {
        hints->ai_family = AF_UNIX;
    } else if (cfg->IPv6) {
        hints->ai_family = AF_INET6;
    } else if (cfg->IPv4) {
        hints->ai_family = AF_INET;
    } else {
        hints->ai_family = AF_UNSPEC;
    }

    if (cfg->datagram) {
        hints->ai_socktype = SOCK_DGRAM;
    } else {
        hints->ai_socktype = SOCK_STREAM;
    }

    /* Set passive unless UDP client */
    if (!cfg->datagram || cfg->server) {
        hints->ai_flags = AI_PASSIVE;
    }

    if (cfg->IPv6 || cfg->IPv4) {
        hints->ai_flags |= AI_NUMERICHOST;
    }
}

static bool set_defaults_and_check_cfg(socket99_config *cfg) {
    if (cfg->backlog_size == 0) { cfg->backlog_size = DEF_BACKLOG_SIZE; }

    /* Screen out contradictory settings */
    if (cfg->IPv6 && cfg->IPv4) { return false; }
    return true;
}

static bool fail_with_errno(socket99_result *out, enum socket99_status status) {
    out->status = status;
    out->saved_errno = errno;
    errno = 0;
    return false;
}

static bool make_unixdomain(socket99_config *cfg, socket99_result *out) {
    int fd = socket(AF_UNIX, cfg->datagram ? SOCK_DGRAM : SOCK_STREAM, 0);
    if (fd == -1) {
        return fail_with_errno(out, SOCKET99_ERROR_SOCKET);
    }

    if (!set_socket_options(cfg, out, fd)) {
        return false;
    }

    struct sockaddr_un sun;
    memset(&sun, 0, sizeof(sun));
    sun.sun_family = AF_UNIX;
    size_t name_max = sizeof(sun.sun_path);

    int snprintf_res = snprintf(sun.sun_path, name_max, "%s", cfg->path);
    if ((int)name_max < snprintf_res) {
        return fail_with_errno(out, SOCKET99_ERROR_SNPRINTF);
    }

    if (cfg->server) {
        /* Note: intentionally NOT unlinking the path here. */
        if (0 != bind(fd, (struct sockaddr *) &sun, sizeof(sun))) {
            return fail_with_errno(out, SOCKET99_ERROR_BIND);
        }

        if (!cfg->datagram) {
            if (listen(fd, cfg->backlog_size) != 0) {
                return fail_with_errno(out, SOCKET99_ERROR_LISTEN);
            }
        }
    } else /* client */ {
        if (0 != connect(fd, (struct sockaddr *) &sun, sizeof(sun))) {
            return fail_with_errno(out, SOCKET99_ERROR_CONNECT);
        }
    }
    
    out->status = SOCKET99_OK;
    out->fd = fd;
    return true;
}

#define PORT_STR_BUFSZ 6

static bool make_tcp_udp(socket99_config *cfg, socket99_result *out) {
    struct addrinfo hints;
    struct addrinfo *res = NULL;

    int fd = -1;
    char port_str[PORT_STR_BUFSZ];
    memset(port_str, 0, PORT_STR_BUFSZ);

    socket99_set_hints(cfg, &hints);

    if (PORT_STR_BUFSZ < snprintf(port_str, PORT_STR_BUFSZ,
            "%u", cfg->port)) {
        return fail_with_errno(out, SOCKET99_ERROR_SNPRINTF);
    }

    struct addrinfo *ai = NULL;
    int addr_res = getaddrinfo(cfg->host, port_str, &hints, &res);
    if (addr_res != 0) {
        out->getaddrinfo_error = addr_res;
        freeaddrinfo(res);
        return fail_with_errno(out, SOCKET99_ERROR_GETADDRINFO);
    }
    
    for (ai = res; ai != NULL; ai=ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd == -1) {
            /* Save errno, but will be clobbered if others succeed. */
            out->status = SOCKET99_ERROR_SOCKET;
            out->saved_errno = errno;
            errno = 0;
            continue;
        }

        if (!set_socket_options(cfg, out, fd)) {
            freeaddrinfo(res);
            return false;
        }

        if (cfg->server) {
            int bind_res = bind(fd, res->ai_addr, res->ai_addrlen);
            if (bind_res == -1) {
                freeaddrinfo(res);
                return fail_with_errno(out, SOCKET99_ERROR_BIND);
            }

            if (!cfg->datagram) {
                int listen_res = listen(fd, cfg->backlog_size);
                if (listen_res == -1) {
                    freeaddrinfo(res);
                    return fail_with_errno(out, SOCKET99_ERROR_LISTEN);
                }
            }
            break;
        } else /* client */ {
            if (cfg->datagram) { break; }

            int res = connect(fd, ai->ai_addr, ai->ai_addrlen);
            if (res == 0) {
                break;
            } else {
                close(fd);
                fd = -1;
                out->status = SOCKET99_ERROR_CONNECT;
                continue;
            }
        }
    }

    if (fd == -1) {
        if (out->status == SOCKET99_OK) {
            freeaddrinfo(res);
            return fail_with_errno(out, SOCKET99_ERROR_UNKNOWN);
        } else {
            out->saved_errno = errno;
            errno = 0;
            freeaddrinfo(res);
            return false;
        }
    }

    out->status = SOCKET99_OK;
    freeaddrinfo(res);
    out->saved_errno = 0;
    out->fd = fd;
    return true;
}

static bool set_nonblocking(socket99_result *out) {
    int flags = fcntl(out->fd, F_GETFL, 0);
    if (flags == -1) {
        return fail_with_errno(out, SOCKET99_ERROR_FCNTL);
    }
    if (fcntl(out->fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return fail_with_errno(out, SOCKET99_ERROR_FCNTL);
    }
    return true;
}

static bool set_socket_options(socket99_config *cfg,
        socket99_result *out, int fd) {
    for (int i = 0; i < SOCKET99_MAX_SOCK_OPTS; i++) {
        socket99_sockopt *opt = &cfg->sockopts[i];
        if (opt->option_id == 0) { break; }

        if (setsockopt(fd, SOL_SOCKET, opt->option_id,
                opt->value, opt->value_len) < 0) {
            return fail_with_errno(out, SOCKET99_ERROR_SETSOCKOPT);
        }
    }
    
    return true;
}


static const char *status_key(enum socket99_status s) {
    switch (s) {
    case SOCKET99_OK:
        return "ok";
    case SOCKET99_ERROR_GETADDRINFO:
        return "getaddrinfo";
    case SOCKET99_ERROR_SOCKET:
        return "socket";
    case SOCKET99_ERROR_BIND:
        return "bind";
    case SOCKET99_ERROR_LISTEN:
        return "listen";
    case SOCKET99_ERROR_CONNECT:
        return "connect";
    case SOCKET99_ERROR_FCNTL:
        return "fcntl";
    case SOCKET99_ERROR_SNPRINTF:
        return "snprintf";
    case SOCKET99_ERROR_CONFIGURATION:
        return "configuration";
    case SOCKET99_ERROR_SETSOCKOPT:
        return "setsockopt";
    case SOCKET99_ERROR_UNKNOWN:
    default:
        return "unknown";
    }
}
