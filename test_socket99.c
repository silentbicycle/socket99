#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>

#include "socket99.h"

typedef bool (test_fun)(void);

#define DEF_PORT 8080
#define MAX_NAME 40
typedef struct {
    test_fun *fun;
    char name[MAX_NAME];
    char *descr;
} test_case_info;

typedef struct {
    int port;
} test_case_config;

bool tcp_client(void);
bool tcp_client_nonblocking(void);
bool tcp_server(void);
bool tcp_server_nonblocking(void);
bool udp_client(void);
bool udp_server(void);
bool unix_client_stream(void);
bool unix_client_datagram(void);
bool unix_server_stream(void);
bool unix_server_datagram(void);

ssize_t read_and_print(int fd);

static int port = DEF_PORT;

#define F(X) X, #X
static test_case_info info[] = {
    { F(tcp_client),
      "connect to 127.0.0.1:PORT via TCP and send \"hello\\n\"" },
    { F(tcp_client_nonblocking),
      "connect to 127.0.0.1:PORT via TCP and send \"hello\\n\" (nonblocking)" },
    { F(tcp_server),
      "listen on 127.0.0.1:PORT via TCP and print client's message" },
    //{ tcp_server_def_port, "listen on 127.0.0.1 via TCP and print port and client's messages" },
    { F(tcp_server_nonblocking),
      "listen on 127.0.0.1:PORT via TCP and print client's message" },
    { F(udp_client),
      "connect to 127.0.0.1:PORT via UDP and send \"hello\\n\"" },
    { F(udp_server),
      "listen on 127.0.0.1:PORT via UDP and print client's message" },
    { F(unix_client_stream),
      "connect to 'test_foo' and print \"hello\\n\" (stream)" },
    { F(unix_client_datagram),
      "connect to 'test_foo' and print \"hello\\n\" (datagram)" },
    { F(unix_server_stream),
      "listen on 'test_foo' socket and print clients' message (stream)" },
    { F(unix_server_datagram),
      "listen on 'test_foo' socket and print clients' message (datagram)" },
};
#undef F

#define TEST_CASE_COUNT (sizeof(info) / sizeof(info[0]))

static void usage(char *name) {
    printf("Integration tests for socket library.\n");
    printf("Usage:\n    %s TEST_NAME\n", name);
    printf("where TEST_NAME is one of:\n");
    for (uint16_t i = 0; i < TEST_CASE_COUNT; i++) {
        printf("'%s':\n    %s\n", info[i].name, info[i].descr);
    }
    exit(1);
}

static test_case_info *lookup(char *name) {
    for (size_t i = 0; i < TEST_CASE_COUNT; i++) {
        if (0 == strncmp(name, info[i].name, MAX_NAME)) {
            return &info[i];
        }
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
    } else {
        if (argc > 2) {
            port = atoi(argv[2]);
        }

        test_case_info *tc = lookup(argv[1]);
        if (tc) {
            bool res = tc->fun();
            if (res) {
                printf("pass %s\n", tc->name);
                return 0;
            } else {
                printf("FAIL %s\n", tc->name);
                return 1;
            }
        } else {
            usage(argv[0]);
        }
    }
    return 0;
}

ssize_t read_and_print(int fd) {
    char buf[1024];
    ssize_t received = recv(fd, buf, 1024, 0);

    if (received > 0) {
        buf[received] = '\0';
        printf("Got: '%s'\n", buf);
    } else {
        fprintf(stderr, "recv: %s\n", strerror(errno));
        errno = 0;
    }
    return received;
}


/* Test cases */

bool tcp_client(void) {
    socket99_config cfg = {
        .host = "127.0.0.1",
        .port = port,
    };

    socket99_result res;
    bool ok = socket99_open(&cfg, &res);
    if (!ok) {
        socket99_fprintf(stderr, &res);
        return false;
    }
    
    const char *msg = "hello\n";
    size_t msg_size = strlen(msg);

    ssize_t sent = send(res.fd, msg, msg_size, 0);
    bool pass = ((size_t)sent == msg_size);
    close(res.fd);
    return pass;
}

bool tcp_client_nonblocking(void) {
    socket99_config cfg = {
        .host = "127.0.0.1",
        .port = port,
        .nonblocking = true,
    };

    socket99_result res;
    bool ok = socket99_open(&cfg, &res);
    if (!ok) {
        socket99_fprintf(stderr, &res);
        return false;
    }
    
    const char *msg = "hello\n";
    size_t msg_size = strlen(msg);

    struct pollfd fds[1] = {
        {res.fd, POLLOUT, 0},
    };
    const int TIMEOUT_MSEC = 10 * 1000;

    bool pass = false;

    int pres = poll(fds, 1, TIMEOUT_MSEC);
    if (pres == 1) {
        if (fds[0].revents & POLLOUT) {
            ssize_t sent = send(res.fd, msg, msg_size, 0);
            pass = ((size_t)sent == msg_size);
        } else if (fds[0].revents & POLLERR || fds[0].revents & POLLHUP) {
            printf("poll: POLLERR or POLLHUP\n");
        }
    } else if (pres == -1) {
        printf("poll: %s\n", strerror(errno));
        errno = 0;
    }
    close(res.fd);
    return pass;
}

bool tcp_server(void) {
    int v_true = 1;

    socket99_config cfg = {
        .host = "127.0.0.1",
        .port = port,
        .server = true,
        .sockopts = {
            {SO_REUSEADDR, &v_true, sizeof(v_true)},
        },
    };

    socket99_result res;
    bool ok = socket99_open(&cfg, &res);
    if (!ok) {
        socket99_fprintf(stderr, &res);
        return false;
    }

    struct sockaddr address;
    socklen_t addr_len;
    int client_fd = accept(res.fd, &address, &addr_len);
    if (client_fd == -1) {
        close(res.fd);
        return false;
    }

    ssize_t received = read_and_print(client_fd);
    close(client_fd);
    close(res.fd);

    return (received > 0);
}

bool tcp_server_nonblocking(void) {
    int v_true = 1;

    socket99_config cfg = {
        .host = "127.0.0.1",
        .port = port,
        .server = true,
        .nonblocking = true,
        .sockopts = {
            {SO_REUSEADDR, &v_true, sizeof(v_true)},
        },
    };

    socket99_result res;
    bool ok = socket99_open(&cfg, &res);
    if (!ok) {
        char buf[128];
        /* Example use of socket99_snprintf */
        if (128 < socket99_snprintf(buf, 128, &res)) {
            /* error message too long */
            socket99_fprintf(stderr, &res);
        } else {
            fprintf(stderr, "%s\n", buf);
        }
        return false;
    }

    struct pollfd fds[2];
    fds[0].fd = res.fd;
    fds[0].events = POLLIN;
    
    ssize_t received = 0;
    int poll_fds = 1;
    int client_fd = -1;
    for (;;) {
        int poll_res = poll(fds, poll_fds, 1000 /* msec */);
        if (poll_res > 0) {
            if (fds[0].revents & POLLIN) {
                struct sockaddr address;
                socklen_t addr_len;
                client_fd = accept(res.fd, &address, &addr_len);
                if (client_fd == -1) {
                    if (errno == EAGAIN) {
                        errno = 0;
                        continue;
                    } else {
                        break;
                    }
                } else {
                    fds[1].fd = client_fd;
                    fds[1].events = POLLIN;
                    poll_fds++;
                }
            } else if (fds[0].revents & POLLERR || fds[0].revents & POLLHUP) {
                printf("POLLERR / POLLHUP\n");
                break;
            }

            if (poll_fds > 1) {
                if (fds[1].revents & POLLIN) {
                    char buf[1024];
                    received = recv(fds[1].fd, buf, 1024, 0);
                    
                    if (received > 0) {
                        buf[received] = '\0';
                        printf("Got: '%s'\n", buf);
                        close(client_fd);
                    } else {
                        if (errno == EAGAIN) {
                            errno = 0;
                        } else {
                            fprintf(stderr, "recv: %s\n", strerror(errno));
                            close(client_fd);
                        }
                    }

                    if (received > 0) { break; }
                } else if (fds[1].revents & POLLERR || fds[1].revents & POLLHUP) {
                    printf("POLLERR / POLLHUP\n");
                    close(client_fd);
                    break;
                }
            }
        }
    }

    close(res.fd);
    return (received > 0);
}

bool udp_client(void) {
    int v_true = 1;

    socket99_config cfg = {
        .host = "127.0.0.1",
        .port = port,
        .datagram = true,
        .sockopts = {
            {SO_BROADCAST, &v_true, sizeof(v_true)},
        },
    };

    socket99_result res;
    bool ok = socket99_open(&cfg, &res);
    if (!ok) {
        socket99_fprintf(stderr, &res);
        return false;
    }
    
    const char *msg = "hello\n";
    size_t msg_size = strlen(msg);

    struct addrinfo hints;
    struct addrinfo *ai_res = NULL;
    socket99_set_hints(&cfg, &hints);
    
    char port_str[8];
    if (8< snprintf(port_str, 8, "%zd", port)) {
        close(res.fd);
        return false;  // death of a thousand papercuts
    }

    struct addrinfo *ai = NULL;
    int addr_res = getaddrinfo(cfg.host, port_str, &hints, &ai_res);
    if (addr_res != 0) {
        printf("getaddrinfo: %s\n", gai_strerror(addr_res));
        close(res.fd);
        return false;
    }

    bool pass = false;
    for (ai = ai_res; ai != NULL; ai=ai->ai_next) {
        ssize_t sent = sendto(res.fd, msg, msg_size, 0, ai->ai_addr, ai->ai_addrlen);
        pass = ((size_t)sent == msg_size);
        if (pass) { break; }
    }
    freeaddrinfo(ai_res);
    close(res.fd);
    return pass;
}

bool udp_server(void) {
    int v_true = 1;

    socket99_config cfg = {
        .host = "127.0.0.1",
        .port = port,
        .server = true,
        .datagram = true,
        .sockopts = {
            {SO_REUSEADDR, &v_true, sizeof(v_true)},
        },
    };

    socket99_result res;
    bool ok = socket99_open(&cfg, &res);
    if (!ok) {
        socket99_fprintf(stderr, &res);
        return false;
    }

    char buf[1024];

    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);
    ssize_t received = recvfrom(res.fd, buf, 1024, 0,
        (struct sockaddr *)&client_addr, &addr_len);
    printf("received %zd, errno %d\n", received, errno);

    if (received > 0) {
        buf[received] = '\0';
        printf("Got: '%s'\n", buf);
    }

    close(res.fd);
    return (received > 0);
}

bool unix_client_stream(void) {
    socket99_config cfg = {
        .path = "test_foo",
    };

    socket99_result res;
    bool ok = socket99_open(&cfg, &res);
    if (!ok) {
        socket99_fprintf(stderr, &res);
        return false;
    }

    write(res.fd, "hello\n", 6);

    close(res.fd);
    return true;
}

bool unix_client_datagram(void) {
    socket99_config cfg = {
        .path = "test_foo",
        .datagram = true, // can be datagram or stream
    };

    socket99_result res;
    bool ok = socket99_open(&cfg, &res);
    if (!ok) {
        socket99_fprintf(stderr, &res);
        return false;
    }

    write(res.fd, "hello\n", 6);

    close(res.fd);
    return true;
}

static bool delete_or_ignore(char *path) {
    int ures = unlink(path);
    if (ures == -1) {
        if (errno == ENOENT) {
            errno = 0;
        } else {
            return false;
        }
    }
    return true;
}

bool unix_server_stream(void) {
    socket99_config cfg = {
        .path = "test_foo",
        .server = true,
        // NB. can be datagram or stream
    };

    if (!delete_or_ignore("test_foo")) { return false; }

    socket99_result res;
    bool ok = socket99_open(&cfg, &res);
    if (!ok) {
        socket99_fprintf(stderr, &res);
        return false;
    }

    struct sockaddr address;
    socklen_t addr_len;
    int client_fd = accept(res.fd, &address, &addr_len);
    if (client_fd == -1) {
        close(res.fd);
        printf("accept: %s\n", strerror(errno));
        return false;
    }
    ssize_t received = read_and_print(client_fd);

    close(client_fd);
    close(res.fd);
    
    if (!delete_or_ignore("test_foo")) { return false; }

    return received > 0;
}

bool unix_server_datagram(void) {
    socket99_config cfg = {
        .path = "test_foo",
        .server = true,
        .datagram = true,
    };

    if (!delete_or_ignore("test_foo")) { return false; }

    socket99_result res;
    bool ok = socket99_open(&cfg, &res);
    if (!ok) {
        socket99_fprintf(stderr, &res);
        return false;
    }

    char buf[1024];

    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);
    ssize_t received = recvfrom(res.fd, buf, 1024, 0,
        (struct sockaddr *)&client_addr, &addr_len);
    printf("received %zd, errno %d\n", received, errno);

    if (received > 0) {
        buf[received] = '\0';
        printf("Got: '%s'\n", buf);
    }

    close(res.fd);
    
    if (!delete_or_ignore("test_foo")) { return false; }

    return received > 0;
}
