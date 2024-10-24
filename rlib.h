// RETOOR - Oct 24 2024
// MIT License
// ===========

// Copyright (c) 2024 Retoor

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#ifndef RLIB_H
#define RLIB_H
// BEGIN OF RLIB

/*
 * Line below will be filtered by rmerge
<script language="Javva script" type="woeiii" src="Pony.html" after-tag="after
tag" />
*/

#ifndef RNET_H
#define RNET_H

#if _POSIX_C_SOURCE != 200112L
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#include <sys/socket.h>
#include <sys/types.h>

#include <unistd.h>
#undef _POSIX_C_SOURCE
#define NET_SOCKET_MAX_CONNECTIONS 50000

typedef struct rnet_socket_t {
    int fd;
    char name[50];
    void *data;
    size_t bytes_received;
    size_t bytes_sent;
    bool connected;
    void (*on_read)(struct rnet_socket_t *);
    void (*on_close)(struct rnet_socket_t *);
    void (*on_connect)(struct rnet_socket_t *);
} rnet_socket_t;

typedef struct rnet_select_result_t {
    int server_fd;
    rnet_socket_t **sockets;
    unsigned int socket_count;
} rnet_select_result_t;

typedef struct rnet_server_t {
    int socket_fd;
    rnet_socket_t **sockets;
    unsigned int socket_count;
    unsigned int port;
    unsigned int backlog;
    rnet_select_result_t *select_result;
    int max_fd;
    void (*on_connect)(rnet_socket_t *socket);
    void (*on_close)(rnet_socket_t *socket);
    void (*on_read)(rnet_socket_t *socket);
} rnet_server_t;

void rnet_select_result_free(rnet_select_result_t *result);
int net_socket_accept(int server_fd);
int net_socket_connect(const char *, unsigned int);
int net_socket_init();
rnet_server_t *net_socket_serve(unsigned int port, unsigned int backlog);
rnet_select_result_t *net_socket_select(rnet_server_t *server);
rnet_socket_t *net_socket_wait(rnet_socket_t *socket_fd);
bool net_set_non_blocking(int sock);
bool net_socket_bind(int sock, unsigned int port);
bool net_socket_listen(int sock, unsigned int backlog);
char *net_socket_name(int sock);
size_t net_socket_write(rnet_socket_t *, unsigned char *, size_t);
rnet_socket_t *get_net_socket_by_fd(int);
unsigned char *net_socket_read(rnet_socket_t *, unsigned int buff_size);
void _net_socket_close(int sock);
void net_socket_close(rnet_socket_t *sock);

rnet_server_t *rnet_server_new(int socket_fd, unsigned int port,
                               unsigned int backlog) {
    rnet_server_t *server = malloc(sizeof(rnet_server_t));
    server->socket_fd = socket_fd;
    server->sockets = NULL;
    server->socket_count = 0;
    server->port = port;
    server->backlog = backlog;
    server->max_fd = -1;
    server->select_result = NULL;
    server->on_connect = NULL;
    server->on_close = NULL;
    server->on_read = NULL;
    return server;
}

rnet_server_t *rnet_server_add_socket(rnet_server_t *server,
                                      rnet_socket_t *sock) {
    server->sockets = realloc(server->sockets, sizeof(rnet_socket_t *) *
                                                   (server->socket_count + 1));
    server->sockets[server->socket_count] = sock;
    server->socket_count++;
    sock->on_read = server->on_read;
    sock->on_connect = server->on_connect;
    sock->on_close = server->on_close;
    sock->connected = true;
    return server;
}

rnet_socket_t sockets[NET_SOCKET_MAX_CONNECTIONS] = {0};
unsigned long sockets_connected = 0;
int net_socket_max_fd = 0;
unsigned long sockets_total = 0;
unsigned long sockets_disconnected = 0;
unsigned long sockets_concurrent_record = 0;
unsigned long sockets_errors = 0;

bool net_set_non_blocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl");
        return false;
    }

    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl");
        return false;
    }

    return true;
}

int net_socket_init() {
    int socket_fd = -1;
    memset(sockets, 0, sizeof(sockets));
    int opt = 1;
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed.\n");
        return false;
    }
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed.\n");
        close(socket_fd);
        return false;
    }
    net_set_non_blocking(socket_fd);
    return socket_fd;
}

char *net_socket_name(int fd) {
    rnet_socket_t *rnet_socket = get_net_socket_by_fd(fd);
    if (rnet_socket) {
        return rnet_socket->name;
        ;
    }

    // If socket disconnected or is no client from server
    return NULL;
}

bool net_socket_bind(int socket_fd, unsigned int port) {
    struct sockaddr_in address;

    address.sin_family = AF_INET;         // IPv4
    address.sin_addr.s_addr = INADDR_ANY; // Bind to any available address
    address.sin_port = htons(port);       // Convert port to network byte order

    if (bind(socket_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(socket_fd);
        return false;
    }
    return true;
}

int net_socket_connect(const char *host, unsigned int port) {
    char port_str[10] = {0};
    sprintf(port_str, "%d", port);
    int status;
    int socket_fd = -1;
    struct addrinfo hints;
    struct addrinfo *res;
    struct addrinfo *p;
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return false;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(host, port_str, &hints, &res)) != 0) {
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        if ((socket_fd =
                 socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue;
        }

        if (connect(socket_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(socket_fd);
            continue;
        }

        break;
    }

    if (p == NULL) {
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    return socket_fd;
}

bool net_socket_listen(int socket_fd, unsigned int backlog) {
    if (listen(socket_fd, backlog) < 0) { // '3' is the backlog size
        perror("Listen failed");
        close(socket_fd);
        return false;
    }
    return true;
}

rnet_server_t *net_socket_serve(unsigned int port, unsigned int backlog) {
    signal(SIGPIPE, SIG_IGN);
    int socket_fd = net_socket_init();
    net_socket_bind(socket_fd, port);
    net_socket_listen(socket_fd, backlog);
    return rnet_server_new(socket_fd, port, backlog);
}

int net_socket_accept(int net_socket_server_fd) {
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int new_socket = -1;
    if ((new_socket = accept(net_socket_server_fd, (struct sockaddr *)&address,
                             (socklen_t *)&addrlen)) < 0) {
        close(new_socket);
        return -1;
    } else {

        return new_socket;
    }
}
/*
static void net_socket_stats(WrenVM *vm)
{

    wrenSetSlotNewList(vm, 0);

    wrenSetSlotString(vm, 1, "sockets_total");
    wrenInsertInList(vm, 0, -1, 1);

    wrenSetSlotDouble(vm, 1, (double)sockets_total);
    wrenInsertInList(vm, 0, -1, 1);

    wrenSetSlotString(vm, 1, "sockets_concurrent_record");
    wrenInsertInList(vm, 0, -1, 1);

    wrenSetSlotDouble(vm, 1, (double)sockets_concurrent_record);
    wrenInsertInList(vm, 0, -1, 1);

    wrenSetSlotString(vm, 1, "sockets_connected");
    wrenInsertInList(vm, 0, -1, 1);

    wrenSetSlotDouble(vm, 1, (double)sockets_connected);
    wrenInsertInList(vm, 0, -1, 1);

    wrenSetSlotString(vm, 1, "sockets_disconnected");
    wrenInsertInList(vm, 0, -1, 1);

    wrenSetSlotDouble(vm, 1, (double)sockets_disconnected);
    wrenInsertInList(vm, 0, -1, 1);
}*/

size_t net_socket_write(rnet_socket_t *sock, unsigned char *message,
                        size_t size) {
    ssize_t sent_total = 0;
    ssize_t sent = 0;
    ssize_t to_send = size;
    while ((sent = send(sock->fd, message, to_send, 0))) {
        if (sent == -1) {
            sockets_errors++;
            net_socket_close(sock);
            break;
        }
        if (sent == 0) {
            printf("EDGE CASE?\n");
            exit(1);
            sockets_errors++;
            net_socket_close(sock);
            break;
        }
        sent_total += sent;
        if (sent_total == to_send)
            break;
    }
    return sent_total;
}

unsigned char *net_socket_read(rnet_socket_t *sock, unsigned int buff_size) {
    if (buff_size > 1024 * 1024 + 1) {
        perror("Buffer too big. Maximum is 1024*1024.\n");
        exit(1);
    }
    static unsigned char buffer[1024 * 1024];
    buffer[0] = 0;
    ssize_t received = recv(sock->fd, buffer, buff_size, 0);
    if (received <= 0) {
        buffer[0] = 0;
        net_socket_close(sock);
        if (received < 0) {
            sockets_errors++;
            return NULL;
        }
    }
    buffer[received + 1] = 0;
    sock->bytes_received = received;
    return buffer;
}

rnet_socket_t *net_socket_wait(rnet_socket_t *sock) {
    if (!sock)
        return NULL;
    if (sock->fd == -1)
        return NULL;
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sock->fd, &read_fds);

    int max_socket_fd = sock->fd;
    int activity = select(max_socket_fd + 1, &read_fds, NULL, NULL, NULL);
    if ((activity < 0) && (errno != EINTR)) {
        // perror("Select error");
        net_socket_close(sock);
        return NULL;
    }
    if (FD_ISSET(sock->fd, &read_fds)) {
        return sock;
    }

    return NULL;
}

void rnet_safe_str(char *str, size_t length) {
    if (!str || !length || !*str)
        return;
    for (unsigned int i = 0; i < length; i++) {
        if (str[i] < 32 || str[i] > 126)
            if (str[i] != 0)
                str[i] = '.';
    }
    str[length] = 0;
}

rnet_select_result_t *rnet_new_socket_select_result(int socket_fd) {
    rnet_select_result_t *result =
        (rnet_select_result_t *)malloc(sizeof(rnet_select_result_t));
    memset(result, 0, sizeof(rnet_select_result_t));
    result->server_fd = socket_fd;
    result->socket_count = 0;
    result->sockets = NULL;
    return result;
}

void rnet_select_result_add(rnet_select_result_t *result, rnet_socket_t *sock) {
    result->sockets = realloc(result->sockets, sizeof(rnet_socket_t *) *
                                                   (result->socket_count + 1));
    result->sockets[result->socket_count] = sock;
    result->socket_count++;
}
void rnet_select_result_free(rnet_select_result_t *result) { free(result); }
rnet_select_result_t *net_socket_select(rnet_server_t *server) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(server->socket_fd, &read_fds);

    server->max_fd = server->socket_fd;
    int socket_fd = -1;
    for (unsigned int i = 0; i < server->socket_count; i++) {
        socket_fd = server->sockets[i]->fd;
        if (!server->sockets[i]->connected) {
            continue;
        }
        if (socket_fd > 0) {
            FD_SET(socket_fd, &read_fds);
            if (socket_fd > server->max_fd) {
                server->max_fd = socket_fd;
            }
        }
    }
    int new_socket = -1;
    struct sockaddr_in address;
    int addrlen = sizeof(struct sockaddr_in);
    int activity = select(server->max_fd + 1, &read_fds, NULL, NULL, NULL);
    if ((activity < 0) && (errno != EINTR)) {
        perror("Select error\n");
        return NULL;
    }
    if (FD_ISSET(server->socket_fd, &read_fds)) {
        if ((new_socket = accept(server->socket_fd, (struct sockaddr *)&address,
                                 (socklen_t *)&addrlen)) < 0) {
            perror("Accept failed\n");
            return NULL;
        }

        // net_set_non_blocking(new_socket);
        char name[50] = {0};
        sprintf(name, "fd:%.4d:ip:%12s:port:%.6d", new_socket,
                inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        rnet_socket_t *sock_obj = NULL;
        for (unsigned int i = 0; i < server->socket_count; i++) {
            if (server->sockets && server->sockets[i]->fd == -1) {
                sock_obj = server->sockets[i];
            }
        }
        if (!sock_obj) {
            sock_obj = (rnet_socket_t *)malloc(sizeof(rnet_socket_t));
            rnet_server_add_socket(server, sock_obj);
        }
        sock_obj->fd = new_socket;
        strcpy(sock_obj->name, name);
        sockets_connected++;
        sockets_total++;
        sockets_concurrent_record =
            sockets_connected > sockets_concurrent_record
                ? sockets_connected
                : sockets_concurrent_record;
        if (new_socket > net_socket_max_fd) {
            net_socket_max_fd = new_socket;
        }
        sock_obj->connected = true;
        sock_obj->on_connect(sock_obj);
    }
    rnet_select_result_t *result =
        rnet_new_socket_select_result(server->socket_fd);
    unsigned int readable_count = 0;
    for (unsigned int i = 0; i < server->socket_count; i++) {
        if (server->sockets[i]->fd == -1)
            continue;
        if (FD_ISSET(server->sockets[i]->fd, &read_fds)) {
            rnet_select_result_add(result, server->sockets[i]);
            readable_count++;
            if (server->sockets[i]->on_read) {
                server->sockets[i]->on_read(server->sockets[i]);
            }
        }
    }
    if (server->select_result) {
        rnet_select_result_free(server->select_result);
        server->select_result = NULL;
    }
    if (readable_count == 0)
        rnet_select_result_free(result);
    return readable_count ? result : NULL;
}

rnet_socket_t *get_net_socket_by_fd(int sock) {
    for (int i = 0; i < net_socket_max_fd; i++) {
        if (sockets[i].fd == sock) {
            return &sockets[i];
        }
    }
    return NULL;
}

void _net_socket_close(int sock) {
    if (sock > 0) {
        sockets_connected--;
        sockets_disconnected++;
        if (sock > 0) {
            if (close(sock) == -1) {
                perror("Error closing socket.\n");
            }
        }
    }
}

void net_socket_close(rnet_socket_t *sock) {
    sock->connected = false;
    if (sock->on_close)
        sock->on_close(sock);
    _net_socket_close(sock->fd);
    sock->fd = -1;
}
#undef _POSIX_C_SOURCE
#endif

#ifndef RTYPES_H
#define RTYPES_H
#include <stdbool.h>
#include <stdint.h> // uint
#include <string.h>
#include <sys/types.h> // ulong
#define ulonglong unsigned long long
#ifndef uint
typedef unsigned int uint;
#endif
#ifndef byte
typedef unsigned char byte;
#endif
#endif

#ifndef RCAT_H
#define RCAT_H
#include <stdio.h>
#include <stdlib.h>

void rcat(char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        printf("rcat: couldn't open \"%s\" for read.\n", filename);
        return;
    }
    unsigned char c;
    while ((c = fgetc(f)) && !feof(f)) {
        printf("%c", c);
    }
    fclose(f);
    fflush(stdout);
}

int rcat_main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: [filename]\n");
        return 1;
    }
    rcat(argv[1]);
    return 0;
}

#endif

#ifndef RCOV_H
#define RCOV_H
#include <pthread.h>
#ifndef RTEMPC_SLOT_COUNT
#define RTEMPC_SLOT_COUNT 20
#endif
#ifndef RTEMPC_SLOT_SIZE
#define RTEMPC_SLOT_SIZE 1024 * 64 * 128
#endif

bool _rtempc_initialized = 0;
pthread_mutex_t _rtempc_thread_lock;
bool rtempc_use_mutex = true;
byte _current_rtempc_slot = 1;
char _rtempc_buffer[RTEMPC_SLOT_COUNT][RTEMPC_SLOT_SIZE];
char *rtempc(char *data) {

    if (rtempc_use_mutex) {
        if (!_rtempc_initialized) {
            _rtempc_initialized = true;
            pthread_mutex_init(&_rtempc_thread_lock, NULL);
        }

        pthread_mutex_lock(&_rtempc_thread_lock);
    }

    uint current_rtempc_slot = _current_rtempc_slot;
    _rtempc_buffer[current_rtempc_slot][0] = 0;
    strcpy(_rtempc_buffer[current_rtempc_slot], data);
    _current_rtempc_slot++;
    if (_current_rtempc_slot == RTEMPC_SLOT_COUNT) {
        _current_rtempc_slot = 0;
    }
    if (rtempc_use_mutex)
        pthread_mutex_unlock(&_rtempc_thread_lock);
    return _rtempc_buffer[current_rtempc_slot];
}

#define sstring(_pname, _psize)                                                \
    static char _##_pname[_psize];                                             \
    _##_pname[0] = 0;                                                          \
    char *_pname = _##_pname;

#define string(_pname, _psize)                                                 \
    char _##_pname[_psize];                                                    \
    _##_pname[0] = 0;                                                          \
    char *_pname = _##_pname;

#define sreset(_pname, _psize) _pname = _##_pname;

#define sbuf(val) rtempc(val)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef RBENCH_H
#define RBENCH_H

#ifndef RPRINT_H
#define RPRINT_H

#ifndef RLIB_TIME
#define RLIB_TIME

#ifndef _POSIX_C_SOURCE_199309L

#define _POSIX_C_SOURCE_199309L
#endif

#include <sys/time.h>

#include <time.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

typedef uint64_t nsecs_t;
void nsleep(nsecs_t nanoseconds);

void tick() { nsleep(1); }

typedef unsigned long long msecs_t;

nsecs_t nsecs() {
    unsigned int lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

msecs_t rnsecs_to_msecs(nsecs_t nsecs) { return nsecs / 1000 / 1000; }

nsecs_t rmsecs_to_nsecs(msecs_t msecs) { return msecs * 1000 * 1000; }

msecs_t usecs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec) * 1000000 + (long long)(tv.tv_usec);
}

msecs_t msecs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec) * 1000 + (tv.tv_usec / 1000);
}
char *msecs_strs(msecs_t ms) {
    static char str[22];
    str[0] = 0;
    sprintf(str, "%f", ms * 0.001);
    for (int i = strlen(str); i > 0; i--) {
        if (str[i] > '0')
            break;
        str[i] = 0;
    }
    return str;
}
char *msecs_strms(msecs_t ms) {
    static char str[22];
    str[0] = 0;
    sprintf(str, "%lld", ms);
    return str;
}
char *msecs_str(long long ms) {
    static char result[30];
    result[0] = 0;
    if (ms > 999) {
        char *s = msecs_strs(ms);
        sprintf(result, "%ss", s);
    } else {
        char *s = msecs_strms(ms);
        sprintf(result, "%sMs", s);
    }
    return result;
}

void nsleep(nsecs_t nanoseconds) {
    long seconds = 0;
    int factor = 0;
    while (nanoseconds > 1000000000) {
        factor++;
        nanoseconds = nanoseconds / 10;
    }
    if (factor) {
        seconds = 1;
        factor--;
        while (factor) {
            seconds = seconds * 10;
            factor--;
        }
    }

    struct timespec req = {seconds, nanoseconds};
    struct timespec rem;

    nanosleep(&req, &rem);
}

void ssleep(double s) {
    long nanoseconds = (long)(1000000000 * s);

    // long seconds = 0;

    // struct timespec req = {seconds, nanoseconds};
    // struct timespec rem;

    nsleep(nanoseconds);
}
void msleep(long miliseonds) {
    long nanoseconds = miliseonds * 1000000;
    nsleep(nanoseconds);
}

char *format_time(int64_t nanoseconds) {
    static char output[1024];
    size_t output_size = sizeof(output);
    output[0] = 0;
    if (nanoseconds < 1000) {
        // Less than 1 microsecond
        snprintf(output, output_size, "%ldns", nanoseconds);
    } else if (nanoseconds < 1000000) {
        // Less than 1 millisecond
        double us = nanoseconds / 1000.0;
        snprintf(output, output_size, "%.2fµs", us);
    } else if (nanoseconds < 1000000000) {
        // Less than 1 second
        double ms = nanoseconds / 1000000.0;
        snprintf(output, output_size, "%.2fms", ms);
    } else {
        // 1 second or more
        double s = nanoseconds / 1000000000.0;
        if (s > 60 * 60) {
            s = s / 60 / 60;
            snprintf(output, output_size, "%.2fh", s);
        } else if (s > 60) {
            s = s / 60;
            snprintf(output, output_size, "%.2fm", s);
        } else {
            snprintf(output, output_size, "%.2fs", s);
        }
    }
    return output;
}

#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

long rpline_number = 0;
nsecs_t rprtime = 0;

int8_t _env_rdisable_colors = -1;
bool _rprint_enable_colors = true;

bool rprint_is_color_enabled() {
    if (_env_rdisable_colors == -1) {
        _env_rdisable_colors = getenv("RDISABLE_COLORS") != NULL;
    }
    if (_env_rdisable_colors) {
        _rprint_enable_colors = false;
    }
    return _rprint_enable_colors;
}

void rprint_disable_colors() { _rprint_enable_colors = false; }
void rprint_enable_colors() { _rprint_enable_colors = true; }
void rprint_toggle_colors() { _rprint_enable_colors = !_rprint_enable_colors; }

void rclear() { printf("\033[2J"); }

void rprintpf(FILE *f, const char *prefix, const char *format, va_list args) {
    char *pprefix = (char *)prefix;
    char *pformat = (char *)format;
    bool reset_color = false;
    bool press_any_key = false;
    char new_format[4096];
    bool enable_color = rprint_is_color_enabled();
    memset(new_format, 0, 4096);
    int new_format_length = 0;
    char temp[1000];
    memset(temp, 0, 1000);
    if (enable_color && pprefix[0]) {
        strcat(new_format, pprefix);
        new_format_length += strlen(pprefix);
        reset_color = true;
    }
    while (true) {
        if (pformat[0] == '\\' && pformat[1] == 'i') {
            strcat(new_format, "\e[3m");
            new_format_length += strlen("\e[3m");
            reset_color = true;
            pformat++;
            pformat++;
        } else if (pformat[0] == '\\' && pformat[1] == 'u') {
            strcat(new_format, "\e[4m");
            new_format_length += strlen("\e[4m");
            reset_color = true;
            pformat++;
            pformat++;
        } else if (pformat[0] == '\\' && pformat[1] == 'b') {
            strcat(new_format, "\e[1m");
            new_format_length += strlen("\e[1m");
            reset_color = true;
            pformat++;
            pformat++;
        } else if (pformat[0] == '\\' && pformat[1] == 'C') {
            press_any_key = true;
            rpline_number++;
            pformat++;
            pformat++;
            reset_color = false;
        } else if (pformat[0] == '\\' && pformat[1] == 'k') {
            press_any_key = true;
            rpline_number++;
            pformat++;
            pformat++;
        } else if (pformat[0] == '\\' && pformat[1] == 'c') {
            rpline_number++;
            strcat(new_format, "\e[2J\e[H");
            new_format_length += strlen("\e[2J\e[H");
            pformat++;
            pformat++;
        } else if (pformat[0] == '\\' && pformat[1] == 'L') {
            rpline_number++;
            temp[0] = 0;
            sprintf(temp, "%ld", rpline_number);
            strcat(new_format, temp);
            new_format_length += strlen(temp);
            pformat++;
            pformat++;
        } else if (pformat[0] == '\\' && pformat[1] == 'l') {
            rpline_number++;
            temp[0] = 0;
            sprintf(temp, "%.5ld", rpline_number);
            strcat(new_format, temp);
            new_format_length += strlen(temp);
            pformat++;
            pformat++;
        } else if (pformat[0] == '\\' && pformat[1] == 'T') {
            nsecs_t nsecs_now = nsecs();
            nsecs_t end = rprtime ? nsecs_now - rprtime : 0;
            temp[0] = 0;
            sprintf(temp, "%s", format_time(end));
            strcat(new_format, temp);
            new_format_length += strlen(temp);
            rprtime = nsecs_now;
            pformat++;
            pformat++;
        } else if (pformat[0] == '\\' && pformat[1] == 't') {
            rprtime = nsecs();
            pformat++;
            pformat++;
        } else {
            new_format[new_format_length] = *pformat;
            new_format_length++;
            if (!*pformat)
                break;

            // printf("%c",*pformat);
            pformat++;
        }
    }
    if (reset_color) {
        strcat(new_format, "\e[0m");
        new_format_length += strlen("\e[0m");
    }

    new_format[new_format_length] = 0;
    vfprintf(f, new_format, args);

    fflush(stdout);
    if (press_any_key) {
        nsecs_t s = nsecs();
        fgetc(stdin);
        rprtime += nsecs() - s;
    }
}

void rprintp(const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(stdout, "", format, args);
    va_end(args);
}

void rprintf(FILE *f, const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(f, "", format, args);
    va_end(args);
}
void rprint(const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(stdout, "", format, args);
    va_end(args);
}
#define printf rprint

// Print line
void rprintlf(FILE *f, const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(f, "\\l", format, args);
    va_end(args);
}
void rprintl(const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(stdout, "\\l", format, args);
    va_end(args);
}

// Black
void rprintkf(FILE *f, const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(f, "\e[30m", format, args);
    va_end(args);
}
void rprintk(const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(stdout, "\e[30m", format, args);
    va_end(args);
}

// Red
void rprintrf(FILE *f, const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(f, "\e[31m", format, args);
    va_end(args);
}
void rprintr(const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(stdout, "\e[31m", format, args);
    va_end(args);
}

// Green
void rprintgf(FILE *f, const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(f, "\e[32m", format, args);
    va_end(args);
}
void rprintg(const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(stdout, "\e[32m", format, args);
    va_end(args);
}

// Yellow
void rprintyf(FILE *f, const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(f, "\e[33m", format, args);
    va_end(args);
}
void rprinty(const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(stdout, "\e[33m", format, args);
    va_end(args);
}

// Blue
void rprintbf(FILE *f, const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(f, "\e[34m", format, args);
    va_end(args);
}

void rprintb(const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(stdout, "\e[34m", format, args);
    va_end(args);
}

// Magenta
void rprintmf(FILE *f, const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(f, "\e[35m", format, args);
    va_end(args);
}
void rprintm(const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(stdout, "\e[35m", format, args);
    va_end(args);
}

// Cyan
void rprintcf(FILE *f, const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(f, "\e[36m", format, args);
    va_end(args);
}
void rprintc(const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(stdout, "\e[36m", format, args);
    va_end(args);
}

// White
void rprintwf(FILE *f, const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(f, "\e[37m", format, args);
    va_end(args);
}
void rprintw(const char *format, ...) {
    va_list args;
    va_start(args, format);
    rprintpf(stdout, "\e[37m", format, args);
    va_end(args);
}
#endif
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#ifndef RSTRING_H
#define RSTRING_H
#ifndef RMALLOC_H
#define RMALLOC_H
#ifndef RMALLOC_OVERRIDE
#define RMALLOC_OVERRIDE 1
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ulonglong rmalloc_count = 0;
ulonglong rmalloc_alloc_count = 0;
ulonglong rmalloc_free_count = 0;

char *rstrdup(const char *s) {
    if (!s)
        return NULL;

    char *result;
    rmalloc_count++;
    rmalloc_alloc_count++;
    size_t size = strlen(s) + 1;
    while (!(result = (char *)malloc(size))) {
        fprintf(stderr, "Warning: strdup failed, trying again.\n");
    }
    memcpy(result, s, size);
    return result;
}
void *rmalloc(size_t size) {
    rmalloc_count++;
    rmalloc_alloc_count++;
    void *result;
    while (!(result = malloc(size))) {
        fprintf(stderr, "Warning: malloc failed, trying again.\n");
    }
    return result;
}
void *rrealloc(void *obj, size_t size) {
    if (obj == NULL) {
        rmalloc_count++;
        rmalloc_count++;
    }
    void *result;
    while (!(result = realloc(obj, size))) {
        fprintf(stderr, "Warning: realloc failed, trying again.\n");
    }
    return result;
}
void *rfree(void *obj) {
    rmalloc_count--;
    rmalloc_free_count++;
    free(obj);
    return NULL;
}

#if RMALLOC_OVERRIDE
#define malloc rmalloc
#define realloc rrealloc
#define free rfree
#define strdup rstrdup
#endif

char *rmalloc_stats() {
    static char res[200];
    res[0] = 0;
    sprintf(res, "Memory usage: %lld allocated, %lld freed, %lld in use.",
            rmalloc_alloc_count, rmalloc_free_count, rmalloc_count);
    return res;
}

#endif

#ifndef RMATH_H
#define RMATH_H
#include <math.h>

#ifndef ceil
double ceil(double x) {
    if (x == (double)(long long)x) {
        return x;
    } else if (x > 0.0) {
        return (double)(long long)x + 1.0;
    } else {
        return (double)(long long)x;
    }
}
#endif

#ifndef floor
double floor(double x) {
    if (x >= 0.0) {
        return (double)(long long)x;
    } else {
        double result = (double)(long long)x;
        return (result == x) ? result : result - 1.0;
    }
}
#endif

#ifndef modf
double modf(double x, double *iptr) {
    double int_part = (x >= 0.0) ? floor(x) : ceil(x);
    *iptr = int_part;
    return x - int_part;
}
#endif
#endif
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

char *rstrtimestamp() {
    time_t current_time;
    time(&current_time);
    struct tm *local_time = localtime(&current_time);
    static char time_string[100];
    time_string[0] = 0;
    strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", local_time);
    return time_string;
}

ulonglong _r_generate_key_current = 0;

char *_rcat_int_int(int a, int b) {
    static char res[20];
    res[0] = 0;
    sprintf(res, "%d%d", a, b);
    return res;
}
char *_rcat_int_double(int a, double b) {
    static char res[20];
    res[0] = 0;
    sprintf(res, "%d%f", a, b);
    return res;
}

char *_rcat_charp_int(char *a, int b) {
    char res[20];
    sprintf(res, "%c", b);
    return strcat(a, res);
}

char *_rcat_charp_double(char *a, double b) {
    char res[20];
    sprintf(res, "%f", b);
    return strcat(a, res);
}

char *_rcat_charp_charp(char *a, char *b) {
    ;
    return strcat(a, b);
}
char *_rcat_charp_char(char *a, char b) {
    char extra[] = {b, 0};
    return strcat(a, extra);
}
char *_rcat_charp_bool(char *a, bool *b) {
    if (b) {
        return strcat(a, "true");
    } else {
        return strcat(a, "false");
    }
}

#define rcat(x, y)                                                             \
    _Generic((x),                                                              \
        int: _Generic((y),                                                     \
        int: _rcat_int_int,                                                    \
        double: _rcat_int_double,                                              \
        char *: _rcat_charp_charp),                                            \
        char *: _Generic((y),                                                  \
        int: _rcat_charp_int,                                                  \
        double: _rcat_charp_double,                                            \
        char *: _rcat_charp_charp,                                             \
        char: _rcat_charp_char,                                                \
        bool: _rcat_charp_bool))((x), (y))

char *rgenerate_key() {
    _r_generate_key_current++;
    static char key[100];
    key[0] = 0;
    sprintf(key, "%lld", _r_generate_key_current);
    return key;
}

char *rformat_number(long long lnumber) {
    static char formatted[1024];

    char number[1024] = {0};
    sprintf(number, "%lld", lnumber);

    int len = strlen(number);
    int commas_needed = (len - 1) / 3;
    int new_len = len + commas_needed;

    formatted[new_len] = '\0';

    int i = len - 1;
    int j = new_len - 1;
    int count = 0;

    while (i >= 0) {
        if (count == 3) {
            formatted[j--] = '.';
            count = 0;
        }
        formatted[j--] = number[i--];
        count++;
    }
    if (lnumber < 0)
        formatted[j--] = '-';
    return formatted;
}

bool rstrextractdouble(char *str, double *d1) {
    for (size_t i = 0; i < strlen(str); i++) {
        if (isdigit(str[i])) {
            str += i;
            sscanf(str, "%lf", d1);
            return true;
        }
    }
    return false;
}

void rstrstripslashes(const char *content, char *result) {
    size_t content_length = strlen((char *)content);
    unsigned int index = 0;
    for (unsigned int i = 0; i < content_length; i++) {
        char c = content[i];
        if (c == '\\') {
            i++;
            c = content[i];
            if (c == 'r') {
                c = '\r';
            } else if (c == 't') {
                c = '\t';
            } else if (c == 'b') {
                c = '\b';
            } else if (c == 'n') {
                c = '\n';
            } else if (c == 'f') {
                c = '\f';
            } else if (c == '\\') {
                // No need tbh
                c = '\\';
            }
        }
        result[index] = c;
        index++;
    }
    result[index] = 0;
}

int rstrstartswith(const char *s1, const char *s2) {
    if (s1 == NULL)
        return s2 == NULL;
    if (s1 == s2 || s2 == NULL || *s2 == 0)
        return true;
    size_t len_s2 = strlen(s2);
    size_t len_s1 = strlen(s1);
    if (len_s2 > len_s1)
        return false;
    return !strncmp(s1, s2, len_s2);
}

bool rstrendswith(const char *s1, const char *s2) {
    if (s1 == NULL)
        return s2 == NULL;
    if (s1 == s2 || s2 == NULL || *s2 == 0)
        return true;
    size_t len_s2 = strlen(s2);
    size_t len_s1 = strlen(s1);
    if (len_s2 > len_s1) {
        return false;
    }
    s1 += len_s1 - len_s2;
    return !strncmp(s1, s2, len_s2);
}

void rstraddslashes(const char *content, char *result) {
    size_t content_length = strlen((char *)content);
    unsigned int index = 0;
    for (unsigned int i = 0; i < content_length; i++) {
        if (content[i] == '\r') {
            result[index] = '\\';
            index++;
            result[index] = 'r';
            index++;
            continue;
        } else if (content[i] == '\t') {
            result[index] = '\\';
            index++;
            result[index] = 't';
            index++;
            continue;
        } else if (content[i] == '\n') {
            result[index] = '\\';
            index++;
            result[index] = 'n';
            index++;
            continue;
        } else if (content[i] == '\\') {
            result[index] = '\\';
            index++;
            result[index] = '\\';
            index++;
            continue;
        } else if (content[i] == '\b') {
            result[index] = '\\';
            index++;
            result[index] = 'b';
            index++;
            continue;
        } else if (content[i] == '\f') {
            result[index] = '\\';
            index++;
            result[index] = 'f';
            index++;
            continue;
        }
        result[index] = content[i];
        index++;
    }
    result[index] = 0;
}

int rstrip_whitespace(char *input, char *output) {
    output[0] = 0;
    int count = 0;
    size_t len = strlen(input);
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '\t' || input[i] == ' ' || input[i] == '\n') {
            continue;
        }
        count = i;
        size_t j;
        for (j = 0; j < len - count; j++) {
            output[j] = input[j + count];
        }
        output[j] = '\0';
        break;
    }
    return count;
}

/*
 * Converts "pony" to \"pony\". Addslashes does not
 * Converts "pony\npony" to "pony\n"
 * 			    "pony"
 */
void rstrtocstring(const char *input, char *output) {
    int index = 0;
    char clean_input[strlen(input) * 2];
    char *iptr = clean_input;
    rstraddslashes(input, clean_input);
    output[index] = '"';
    index++;
    while (*iptr) {
        if (*iptr == '"') {
            output[index] = '\\';
            output++;
        } else if (*iptr == '\\' && *(iptr + 1) == 'n') {
            output[index] = '\\';
            output++;
            output[index] = 'n';
            output++;
            output[index] = '"';
            output++;
            output[index] = '\n';
            output++;
            output[index] = '"';
            output++;
            iptr++;
            iptr++;
            continue;
        }
        output[index] = *iptr;
        index++;
        iptr++;
    }
    if (output[index - 1] == '"' && output[index - 2] == '\n') {
        output[index - 1] = 0;
    } else if (output[index - 1] != '"') {
        output[index] = '"';
        output[index + 1] = 0;
    }
}

size_t rstrtokline(char *input, char *output, size_t offset, bool strip_nl) {

    size_t len = strlen(input);
    output[0] = 0;
    size_t new_offset = 0;
    size_t j;
    size_t index = 0;

    for (j = offset; j < len + offset; j++) {
        if (input[j] == 0) {
            index++;
            break;
        }
        index = j - offset;
        output[index] = input[j];

        if (output[index] == '\n') {
            index++;
            break;
        }
    }
    output[index] = 0;

    new_offset = index + offset;

    if (strip_nl) {
        if (output[index - 1] == '\n') {
            output[index - 1] = 0;
        }
    }
    return new_offset;
}

void rstrjoin(char **lines, size_t count, char *glue, char *output) {
    output[0] = 0;
    for (size_t i = 0; i < count; i++) {
        strcat(output, lines[i]);
        if (i != count - 1)
            strcat(output, glue);
    }
}

int rstrsplit(char *input, char **lines) {
    int index = 0;
    size_t offset = 0;
    char line[1024];
    while ((offset = rstrtokline(input, line, offset, false)) && *line) {
        if (!*line) {
            break;
        }
        lines[index] = (char *)malloc(strlen(line) + 1);
        strcpy(lines[index], line);
        index++;
    }
    return index;
}

bool rstartswithnumber(char *str) { return isdigit(str[0]); }

void rstrmove2(char *str, unsigned int start, size_t length,
               unsigned int new_pos) {
    size_t str_len = strlen(str);
    char new_str[str_len + 1];
    memset(new_str, 0, str_len);
    if (start < new_pos) {
        strncat(new_str, str + length, str_len - length - start);
        new_str[new_pos] = 0;
        strncat(new_str, str + start, length);
        strcat(new_str, str + strlen(new_str));
        memset(str, 0, str_len);
        strcpy(str, new_str);
    } else {
        strncat(new_str, str + start, length);
        strncat(new_str, str, start);
        strncat(new_str, str + start + length, str_len - start);
        memset(str, 0, str_len);
        strcpy(str, new_str);
    }
    new_str[str_len] = 0;
}

void rstrmove(char *str, unsigned int start, size_t length,
              unsigned int new_pos) {
    size_t str_len = strlen(str);
    if (start >= str_len || new_pos >= str_len || start + length > str_len) {
        return;
    }
    char temp[length + 1];
    strncpy(temp, str + start, length);
    temp[length] = 0;
    if (start < new_pos) {
        memmove(str + start, str + start + length, new_pos - start);
        strncpy(str + new_pos - length + 1, temp, length);
    } else {
        memmove(str + new_pos + length, str + new_pos, start - new_pos);
        strncpy(str + new_pos, temp, length);
    }
}

int cmp_line(const void *left, const void *right) {
    char *l = *(char **)left;
    char *r = *(char **)right;

    char lstripped[strlen(l) + 1];
    rstrip_whitespace(l, lstripped);
    char rstripped[strlen(r) + 1];
    rstrip_whitespace(r, rstripped);

    double d1, d2;
    bool found_d1 = rstrextractdouble(lstripped, &d1);
    bool found_d2 = rstrextractdouble(rstripped, &d2);

    if (found_d1 && found_d2) {
        double frac_part1;
        double int_part1;
        frac_part1 = modf(d1, &int_part1);
        double frac_part2;
        double int_part2;
        frac_part2 = modf(d2, &int_part2);
        if (d1 == d2) {
            return strcmp(lstripped, rstripped);
        } else if (frac_part1 && frac_part2) {
            return d1 > d2;
        } else if (frac_part1 && !frac_part2) {
            return 1;
        } else if (frac_part2 && !frac_part1) {
            return -1;
        } else if (!frac_part1 && !frac_part2) {
            return d1 > d2;
        }
    }
    return 0;
}

int rstrsort(char *input, char *output) {
    char **lines = (char **)malloc(strlen(input) * 10);
    int line_count = rstrsplit(input, lines);
    qsort(lines, line_count, sizeof(char *), cmp_line);
    rstrjoin(lines, line_count, "", output);
    for (int i = 0; i < line_count; i++) {
        free(lines[i]);
    }
    free(lines);
    return line_count;
}

#endif

#ifndef RLIB_TERMINAL_H
#define RLIB_TERMINAL_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef RTEST_H
#define RTEST_H
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#define debug(fmt, ...) printf("%s:%d: " fmt, __FILE__, __LINE__, __VA_ARGS__);

char *rcurrent_banner;
int rassert_count = 0;
unsigned short rtest_is_first = 1;
unsigned int rtest_fail_count = 0;

int rtest_end(char *content) {
    // Returns application exit code. 0 == success
    printf("%s", content);
    printf("\n@assertions: %d\n", rassert_count);
    printf("@memory: %s\n", rmalloc_stats());

    if (rmalloc_count != 0) {
        printf("MEMORY ERROR\n");
        return rtest_fail_count > 0;
    }
    return rtest_fail_count > 0;
}

void rtest_test_banner(char *content, char *file) {
    if (rtest_is_first == 1) {
        char delimiter[] = ".";
        char *d = delimiter;
        char f[2048];
        strcpy(f, file);
        printf("%s tests", strtok(f, d));
        rtest_is_first = 0;
        setvbuf(stdout, NULL, _IONBF, 0);
    }
    printf("\n - %s ", content);
}

bool rtest_test_true_silent(char *expr, int res, int line) {
    rassert_count++;
    if (res) {
        return true;
    }
    rprintrf(stderr, "\nERROR on line %d: %s", line, expr);
    rtest_fail_count++;
    return false;
}

bool rtest_test_true(char *expr, int res, int line) {
    rassert_count++;
    if (res) {
        fprintf(stdout, ".");
        return true;
    }
    rprintrf(stderr, "\nERROR on line %d: %s", line, expr);
    rtest_fail_count++;
    return false;
}
bool rtest_test_false_silent(char *expr, int res, int line) {
    return rtest_test_true_silent(expr, !res, line);
}
bool rtest_test_false(char *expr, int res, int line) {
    return rtest_test_true(expr, !res, line);
}
void rtest_test_skip(char *expr, int line) {
    rprintgf(stderr, "\n @skip(%s) on line %d\n", expr, line);
}
void rtest_test_assert(char *expr, int res, int line) {
    if (rtest_test_true(expr, res, line)) {
        return;
    }
    rtest_end("");
    exit(40);
}

#define rtest_banner(content)                                                  \
    rcurrent_banner = content;                                                 \
    rtest_test_banner(content, __FILE__);
#define rtest_true(expr) rtest_test_true(#expr, expr, __LINE__);
#define rtest_assert(expr)                                                     \
    {                                                                          \
        int __valid = expr ? 1 : 0;                                            \
        rtest_test_true(#expr, __valid, __LINE__);                             \
    };                                                                         \
    ;

#define rassert(expr)                                                          \
    {                                                                          \
        int __valid = expr ? 1 : 0;                                            \
        rtest_test_true(#expr, __valid, __LINE__);                             \
    };                                                                         \
    ;
#define rtest_asserts(expr)                                                    \
    {                                                                          \
        int __valid = expr ? 1 : 0;                                            \
        rtest_test_true_silent(#expr, __valid, __LINE__);                      \
    };
#define rasserts(expr)                                                         \
    {                                                                          \
        int __valid = expr ? 1 : 0;                                            \
        rtest_test_true_silent(#expr, __valid, __LINE__);                      \
    };
#define rtest_false(expr)                                                      \
    rprintf(" [%s]\t%s\t\n", expr == 0 ? "OK" : "NOK", #expr);                 \
    assert_count++;                                                            \
    assert(#expr);
#define rtest_skip(expr) rtest_test_skip(#expr, __LINE__);

FILE *rtest_create_file(char *path, char *content) {
    FILE *fd = fopen(path, "wb");

    char c;
    int index = 0;

    while ((c = content[index]) != 0) {
        fputc(c, fd);
        index++;
    }
    fclose(fd);
    fd = fopen(path, "rb");
    return fd;
}

void rtest_delete_file(char *path) { unlink(path); }
#endif

char *rfcaptured = NULL;

void rfcapture(FILE *f, char *buff, size_t size) {
    rfcaptured = buff;
    setvbuf(f, rfcaptured, _IOFBF, size);
}
void rfstopcapture(FILE *f) { setvbuf(f, 0, _IOFBF, 0); }

bool _r_disable_stdout_toggle = false;

FILE *_r_original_stdout = NULL;

bool rr_enable_stdout() {
    if (_r_disable_stdout_toggle)
        return false;
    if (!_r_original_stdout) {
        stdout = fopen("/dev/null", "rb");
        return false;
    }
    if (_r_original_stdout && _r_original_stdout != stdout) {
        fclose(stdout);
    }
    stdout = _r_original_stdout;
    return true;
}
bool rr_disable_stdout() {
    if (_r_disable_stdout_toggle) {
        return false;
    }
    if (_r_original_stdout == NULL) {
        _r_original_stdout = stdout;
    }
    if (stdout == _r_original_stdout) {
        stdout = fopen("/dev/null", "rb");
        return true;
    }
    return false;
}
bool rr_toggle_stdout() {
    if (!_r_original_stdout) {
        rr_disable_stdout();
        return true;
    } else if (stdout != _r_original_stdout) {
        rr_enable_stdout();
        return true;
    } else {
        rr_disable_stdout();
        return true;
    }
}

typedef struct rprogressbar_t {
    unsigned long current_value;
    unsigned long min_value;
    unsigned long max_value;
    unsigned int length;
    bool changed;
    double percentage;
    unsigned int width;
    unsigned long draws;
    FILE *fout;
} rprogressbar_t;

rprogressbar_t *rprogressbar_new(long min_value, long max_value,
                                 unsigned int width, FILE *fout) {
    rprogressbar_t *pbar = (rprogressbar_t *)malloc(sizeof(rprogressbar_t));
    pbar->min_value = min_value;
    pbar->max_value = max_value;
    pbar->current_value = min_value;
    pbar->width = width;
    pbar->draws = 0;
    pbar->length = 0;
    pbar->changed = false;
    pbar->fout = fout ? fout : stdout;
    return pbar;
}

void rprogressbar_free(rprogressbar_t *pbar) { free(pbar); }

void rprogressbar_draw(rprogressbar_t *pbar) {
    if (!pbar->changed) {
        return;
    } else {
        pbar->changed = false;
    }
    pbar->draws++;
    char draws_text[22];
    draws_text[0] = 0;
    sprintf(draws_text, "%ld", pbar->draws);
    char *draws_textp = draws_text;
    // bool draws_text_len = strlen(draws_text);
    char bar_begin_char = ' ';
    char bar_progress_char = ' ';
    char bar_empty_char = ' ';
    char bar_end_char = ' ';
    char content[4096] = {0};
    char bar_content[1024];
    char buff[2048] = {0};
    bar_content[0] = '\r';
    bar_content[1] = bar_begin_char;
    unsigned int index = 2;
    for (unsigned long i = 0; i < pbar->length; i++) {
        if (*draws_textp) {
            bar_content[index] = *draws_textp;
            draws_textp++;
        } else {
            bar_content[index] = bar_progress_char;
        }
        index++;
    }
    char infix[] = "\033[0m";
    for (unsigned long i = 0; i < strlen(infix); i++) {
        bar_content[index] = infix[i];
        index++;
    }
    for (unsigned long i = 0; i < pbar->width - pbar->length; i++) {
        bar_content[index] = bar_empty_char;
        index++;
    }
    bar_content[index] = bar_end_char;
    bar_content[index + 1] = '\0';
    sprintf(buff, "\033[43m%s\033[0m \033[33m%.2f%%\033[0m ", bar_content,
            pbar->percentage * 100);
    strcat(content, buff);
    if (pbar->width == pbar->length) {
        strcat(content, "\r");
        for (unsigned long i = 0; i < pbar->width + 10; i++) {
            strcat(content, " ");
        }
        strcat(content, "\r");
    }
    fprintf(pbar->fout, "%s", content);
    fflush(pbar->fout);
}

bool rprogressbar_update(rprogressbar_t *pbar, unsigned long value) {
    if (value == pbar->current_value) {
        return false;
    }
    pbar->current_value = value;
    pbar->percentage = (double)pbar->current_value /
                       (double)(pbar->max_value - pbar->min_value);
    unsigned long new_length = (unsigned long)(pbar->percentage * pbar->width);
    pbar->changed = new_length != pbar->length;
    if (pbar->changed) {
        pbar->length = new_length;
        rprogressbar_draw(pbar);
        return true;
    }
    return false;
}

size_t rreadline(char *data, size_t len, bool strip_ln) {
    __attribute__((unused)) char *unused = fgets(data, len, stdin);
    size_t length = strlen(data);
    if (length && strip_ln)
        data[length - 1] = 0;
    return length;
}

void rlib_test_progressbar() {
    rtest_banner("Progress bar");
    rprogressbar_t *pbar = rprogressbar_new(0, 1000, 10, stderr);
    rprogressbar_draw(pbar);
    // No draws executed, nothing to show
    rassert(pbar->draws == 0);
    rprogressbar_update(pbar, 500);
    rassert(pbar->percentage == 0.5);
    rprogressbar_update(pbar, 500);
    rprogressbar_update(pbar, 501);
    rprogressbar_update(pbar, 502);
    // Should only have drawn one time since value did change, but percentage
    // did not
    rassert(pbar->draws == 1);
    // Changed is false because update function calls draw
    rassert(pbar->changed == false);
    rprogressbar_update(pbar, 777);
    rassert(pbar->percentage == 0.777);
    rprogressbar_update(pbar, 1000);
    rassert(pbar->percentage == 1);
}

#endif

#define RBENCH(times, action)                                                  \
    {                                                                          \
        unsigned long utimes = (unsigned long)times;                           \
        nsecs_t start = nsecs();                                               \
        for (unsigned long i = 0; i < utimes; i++) {                           \
            {                                                                  \
                action;                                                        \
            }                                                                  \
        }                                                                      \
        nsecs_t end = nsecs();                                                 \
        printf("%s\n", format_time(end - start));                              \
    }

#define RBENCHP(times, action)                                                 \
    {                                                                          \
        printf("\n");                                                          \
        nsecs_t start = nsecs();                                               \
        unsigned int prev_percentage = 0;                                      \
        unsigned long utimes = (unsigned long)times;                           \
        for (unsigned long i = 0; i < utimes; i++) {                           \
            unsigned int percentage =                                          \
                ((long double)i / (long double)times) * 100;                   \
            int percentage_changed = percentage != prev_percentage;            \
            __attribute__((unused)) int first = i == 0;                        \
            __attribute__((unused)) int last = i == utimes - 1;                \
            { action; };                                                       \
            if (percentage_changed) {                                          \
                printf("\r%d%%", percentage);                                  \
                fflush(stdout);                                                \
                                                                               \
                prev_percentage = percentage;                                  \
            }                                                                  \
        }                                                                      \
        nsecs_t end = nsecs();                                                 \
        printf("\r%s\n", format_time(end - start));                            \
    }

struct rbench_t;

typedef struct rbench_function_t {
#ifdef __cplusplus
    void (*call)();
#else
    void(*call);
#endif
    char name[256];
    char group[256];
    void *arg;
    void *data;
    bool first;
    bool last;
    int argc;
    unsigned long times_executed;

    nsecs_t average_execution_time;
    nsecs_t total_execution_time;
} rbench_function_t;

typedef struct rbench_t {
    unsigned int function_count;
    rbench_function_t functions[100];
    rbench_function_t *current;
    rprogressbar_t *progress_bar;
    bool show_progress;
    int winner;
    bool stdout;
    unsigned long times;
    bool silent;
    nsecs_t execution_time;
#ifdef __cplusplus
    void (*add_function)(struct rbench_t *r, const char *name,
                         const char *group, void (*)());
#else
    void (*add_function)(struct rbench_t *r, const char *name,
                         const char *group, void *);
#endif
    void (*rbench_reset)(struct rbench_t *r);
    struct rbench_t *(*execute)(struct rbench_t *r, long times);
    struct rbench_t *(*execute1)(struct rbench_t *r, long times, void *arg1);
    struct rbench_t *(*execute2)(struct rbench_t *r, long times, void *arg1,
                                 void *arg2);
    struct rbench_t *(*execute3)(struct rbench_t *r, long times, void *arg1,
                                 void *arg2, void *arg3);

} rbench_t;

FILE *_rbench_stdout = NULL;
FILE *_rbench_stdnull = NULL;

void rbench_toggle_stdout(rbench_t *r) {
    if (!r->stdout) {
        if (_rbench_stdout == NULL) {
            _rbench_stdout = stdout;
        }
        if (_rbench_stdnull == NULL) {
            _rbench_stdnull = fopen("/dev/null", "wb");
        }
        if (stdout == _rbench_stdout) {
            stdout = _rbench_stdnull;
        } else {
            stdout = _rbench_stdout;
        }
    }
}
void rbench_restore_stdout(rbench_t *r) {
    if (r->stdout)
        return;
    if (_rbench_stdout) {
        stdout = _rbench_stdout;
    }
    if (_rbench_stdnull) {
        fclose(_rbench_stdnull);
        _rbench_stdnull = NULL;
    }
}

rbench_t *rbench_new();

rbench_t *_rbench = NULL;
rbench_function_t *rbf;
rbench_t *rbench() {
    if (_rbench == NULL) {
        _rbench = rbench_new();
    }
    return _rbench;
}

typedef void *(*rbench_call)();
typedef void *(*rbench_call1)(void *);
typedef void *(*rbench_call2)(void *, void *);
typedef void *(*rbench_call3)(void *, void *, void *);

#ifdef __cplusplus
void rbench_add_function(rbench_t *rp, const char *name, const char *group,
                         void (*call)()) {
#else
void rbench_add_function(rbench_t *rp, const char *name, const char *group,
                         void *call) {
#endif
    rbench_function_t *f = &rp->functions[rp->function_count];
    rp->function_count++;
    f->average_execution_time = 0;
    f->total_execution_time = 0;
    f->times_executed = 0;
    f->call = call;
    strcpy(f->name, name);
    strcpy(f->group, group);
}

void rbench_reset_function(rbench_function_t *f) {
    f->average_execution_time = 0;
    f->times_executed = 0;
    f->total_execution_time = 0;
}

void rbench_reset(rbench_t *rp) {
    for (unsigned int i = 0; i < rp->function_count; i++) {
        rbench_reset_function(&rp->functions[i]);
    }
}
int rbench_get_winner_index(rbench_t *r) {
    int winner = 0;
    nsecs_t time = 0;
    for (unsigned int i = 0; i < r->function_count; i++) {
        if (time == 0 || r->functions[i].total_execution_time < time) {
            winner = i;
            time = r->functions[i].total_execution_time;
        }
    }
    return winner;
}
bool rbench_was_last_function(rbench_t *r) {
    for (unsigned int i = 0; i < r->function_count; i++) {
        if (i == r->function_count - 1 && r->current == &r->functions[i])
            return true;
    }
    return false;
}

rbench_function_t *rbench_execute_prepare(rbench_t *r, int findex, long times,
                                          int argc) {
    rbench_toggle_stdout(r);
    if (findex == 0) {
        r->execution_time = 0;
    }
    rbench_function_t *rf = &r->functions[findex];
    rf->argc = argc;
    rbf = rf;
    r->current = rf;
    if (r->show_progress)
        r->progress_bar = rprogressbar_new(0, times, 20, stderr);
    r->times = times;
    // printf("   %s:%s gets executed for %ld times with %d
    // arguments.\n",rf->group, rf->name, times,argc);
    rbench_reset_function(rf);

    return rf;
}
void rbench_execute_finish(rbench_t *r) {
    rbench_toggle_stdout(r);
    if (r->progress_bar) {
        free(r->progress_bar);
        r->progress_bar = NULL;
    }
    r->current->average_execution_time =
        r->current->total_execution_time / r->current->times_executed;
    ;
    // printf("   %s:%s finished executing in
    // %s\n",r->current->group,r->current->name,
    // format_time(r->current->total_execution_time));
    // rbench_show_results_function(r->current);
    if (rbench_was_last_function(r)) {
        rbench_restore_stdout(r);
        unsigned int winner_index = rbench_get_winner_index(r);
        r->winner = winner_index + 1;
        if (!r->silent)
            rprintgf(stderr, "Benchmark results:\n");
        nsecs_t total_time = 0;

        for (unsigned int i = 0; i < r->function_count; i++) {
            rbf = &r->functions[i];
            total_time += rbf->total_execution_time;
            bool is_winner = winner_index == i;
            if (is_winner) {
                if (!r->silent)
                    rprintyf(stderr, " > %s:%s:%s\n",
                             format_time(rbf->total_execution_time), rbf->group,
                             rbf->name);
            } else {
                if (!r->silent)
                    rprintbf(stderr, "   %s:%s:%s\n",
                             format_time(rbf->total_execution_time), rbf->group,
                             rbf->name);
            }
        }
        if (!r->silent)
            rprintgf(stderr, "Total execution time: %s\n",
                     format_time(total_time));
    }
    rbench_restore_stdout(r);
    rbf = NULL;
    r->current = NULL;
}
struct rbench_t *rbench_execute(rbench_t *r, long times) {

    for (unsigned int i = 0; i < r->function_count; i++) {

        rbench_function_t *f = rbench_execute_prepare(r, i, times, 0);
        rbench_call c = (rbench_call)f->call;
        nsecs_t start = nsecs();
        f->first = true;
        c();
        f->first = false;
        f->last = false;
        f->times_executed++;
        for (int j = 1; j < times; j++) {
            c();
            f->times_executed++;
            f->last = f->times_executed == r->times - 1;
            if (r->progress_bar) {
                rprogressbar_update(r->progress_bar, f->times_executed);
            }
        }
        f->total_execution_time = nsecs() - start;
        r->execution_time += f->total_execution_time;
        rbench_execute_finish(r);
    }
    return r;
}

struct rbench_t *rbench_execute1(rbench_t *r, long times, void *arg1) {

    for (unsigned int i = 0; i < r->function_count; i++) {
        rbench_function_t *f = rbench_execute_prepare(r, i, times, 1);
        rbench_call1 c = (rbench_call1)f->call;
        nsecs_t start = nsecs();
        f->first = true;
        c(arg1);
        f->first = false;
        f->last = false;
        f->times_executed++;
        for (int j = 1; j < times; j++) {
            c(arg1);
            f->times_executed++;
            f->last = f->times_executed == r->times - 1;
            if (r->progress_bar) {
                rprogressbar_update(r->progress_bar, f->times_executed);
            }
        }
        f->total_execution_time = nsecs() - start;
        r->execution_time += f->total_execution_time;
        rbench_execute_finish(r);
    }
    return r;
}

struct rbench_t *rbench_execute2(rbench_t *r, long times, void *arg1,
                                 void *arg2) {

    for (unsigned int i = 0; i < r->function_count; i++) {
        rbench_function_t *f = rbench_execute_prepare(r, i, times, 2);
        rbench_call2 c = (rbench_call2)f->call;
        nsecs_t start = nsecs();
        f->first = true;
        c(arg1, arg2);
        f->first = false;
        f->last = false;
        f->times_executed++;
        for (int j = 1; j < times; j++) {
            c(arg1, arg2);
            f->times_executed++;
            f->last = f->times_executed == r->times - 1;
            if (r->progress_bar) {
                rprogressbar_update(r->progress_bar, f->times_executed);
            }
        }
        f->total_execution_time = nsecs() - start;
        r->execution_time += f->total_execution_time;
        rbench_execute_finish(r);
    }
    return r;
}

struct rbench_t *rbench_execute3(rbench_t *r, long times, void *arg1,
                                 void *arg2, void *arg3) {

    for (unsigned int i = 0; i < r->function_count; i++) {
        rbench_function_t *f = rbench_execute_prepare(r, i, times, 3);

        rbench_call3 c = (rbench_call3)f->call;
        nsecs_t start = nsecs();
        f->first = true;
        c(arg1, arg2, arg3);
        f->first = false;
        f->last = false;
        f->times_executed++;
        for (int j = 1; j < times; j++) {
            c(arg1, arg2, arg3);
            f->times_executed++;
            f->last = f->times_executed == r->times - 1;
            if (r->progress_bar) {
                rprogressbar_update(r->progress_bar, f->times_executed);
            }
        }
        f->total_execution_time = nsecs() - start;
        rbench_execute_finish(r);
    }
    return r;
}

rbench_t *rbench_new() {

    rbench_t *r = (rbench_t *)malloc(sizeof(rbench_t));
    memset(r, 0, sizeof(rbench_t));
    r->add_function = rbench_add_function;
    r->rbench_reset = rbench_reset;
    r->execute1 = rbench_execute1;
    r->execute2 = rbench_execute2;
    r->execute3 = rbench_execute3;
    r->execute = rbench_execute;
    r->stdout = true;
    r->silent = false;
    r->winner = 0;
    r->show_progress = true;
    return r;
}
void rbench_free(rbench_t *r) { free(r); }

#endif
bool check_lcov() {
    char buffer[1024 * 64];
    FILE *fp;
    fp = popen("lcov --help", "r");
    if (fp == NULL) {
        return false;
    }
    if (fgets(buffer, sizeof(buffer), fp) == NULL) {
        return false;
    }
    pclose(fp);
    return strstr(buffer, "lcov: not found") ? false : true;
}

int rcov_main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: [source.c]\n");
        return 1;
    }
    char argstr[4096] = {0};
    for (int i = 2; i < argc; i++) {
        strcat(argstr, argv[i]);
        strcat(argstr, " ");
    }
    if (!check_lcov()) {

        printf(
            "lcov is not installed. Please execute `sudo apt install lcov`.\n");
        return 1;
    }
    char *source_file = argv[1];
    char *commands[] = {
        "rm -f *.gcda   2>/dev/null",
        "rm -f *.gcno   2>/dev/null",
        "rm -f %s.coverage.info   2>/dev/null",
        "gcc -pg -fprofile-arcs -ftest-coverage -g -o %s_coverage.o %s",
        "./%s_coverage.o",
        "lcov --capture --directory . --output-file %s.coverage.info",
        "genhtml %s.coverage.info --output-directory /tmp/%s.coverage",
        "rm -f *.gcda   2>/dev/null",
        "rm -f *.gcno   2>/dev/null",
        "rm -f %s.coverage.info   2>/dev/null", //"cat gmon.out",

        "gprof %s_coverage.o gmon.out > output.rcov_analysis",

        "rm -f gmon.out",
        "cat output.rcov_analysis",
        "rm output.rcov_analysis",
        "rm -f %s_coverage.o",

        "google-chrome /tmp/%s.coverage/index.html"};
    uint command_count = sizeof(commands) / sizeof(commands[0]);
    RBENCH(1,{
        for (uint i = 0; i < command_count; i++) {
            char *formatted_command = sbuf("");
            sprintf(formatted_command, commands[i], source_file, source_file);
            // printf("%s\n", formatted_command);
            if (formatted_command[0] == '.' && formatted_command[1] == '/') {
                strcat(formatted_command, " ");
                strcat(formatted_command, argstr);
            }

            if (system(formatted_command)) {
                printf("`%s` returned non-zero code.\n", formatted_command);
            }
        });
    }
    return 0;
}
#endif

#ifndef RHTTP_H
#define RHTTP_H
#ifndef RLIB_RIO
#define RLIB_RIO
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/dir.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef RSTRING_LIST_H
#define RSTRING_LIST_H
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct rstring_list_t {
    unsigned int size;
    unsigned int count;
    char **strings;
} rstring_list_t;

rstring_list_t *rstring_list_new() {
    rstring_list_t *rsl = (rstring_list_t *)malloc(sizeof(rstring_list_t));
    memset(rsl, 0, sizeof(rstring_list_t));
    return rsl;
}

void rstring_list_free(rstring_list_t *rsl) {
    for (unsigned int i = 0; i < rsl->size; i++) {
        free(rsl->strings[i]);
    }
    free(rsl);
    rsl = NULL;
}

void rstring_list_add(rstring_list_t *rsl, char *str) {
    if (rsl->count == rsl->size) {
        rsl->size++;
        rsl->strings =
            (char **)realloc(rsl->strings, sizeof(char *) * rsl->size);
    }
    rsl->strings[rsl->count] = (char *)malloc(strlen(str) + 1);
    strcpy(rsl->strings[rsl->count], str);
    rsl->count++;
}
bool rstring_list_contains(rstring_list_t *rsl, char *str) {
    for (unsigned int i = 0; i < rsl->count; i++) {
        if (!strcmp(rsl->strings[i], str))
            return true;
    }
    return false;
}

#endif

bool rfile_exists(char *path) {
    struct stat s;
    return !stat(path, &s);
}

void rjoin_path(char *p1, char *p2, char *output) {
    output[0] = 0;
    strcpy(output, p1);

    if (output[strlen(output) - 1] != '/') {
        char slash[] = "/";
        strcat(output, slash);
    }
    if (p2[0] == '/') {
        p2++;
    }
    strcat(output, p2);
}

int risprivatedir(const char *path) {
    struct stat statbuf;

    if (stat(path, &statbuf) != 0) {
        perror("stat");
        return -1;
    }

    if (!S_ISDIR(statbuf.st_mode)) {
        return -2;
    }

    if ((statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) == S_IRWXU) {
        return 1; // Private (owner has all permissions, others have none)
    }

    return 0;
}
bool risdir(const char *path) { return !risprivatedir(path); }

void rforfile(char *path, void callback(char *)) {
    if (!rfile_exists(path))
        return;
    DIR *dir = opendir(path);
    struct dirent *d;
    while ((d = readdir(dir)) != NULL) {
        if (!d)
            break;

        if ((d->d_name[0] == '.' && strlen(d->d_name) == 1) ||
            d->d_name[1] == '.') {
            continue;
        }
        char full_path[4096];
        rjoin_path(path, d->d_name, full_path);

        if (risdir(full_path)) {
            callback(full_path);
            rforfile(full_path, callback);
        } else {
            callback(full_path);
        }
    }
    closedir(dir);
}

bool rfd_wait(int fd, int ms) {

    fd_set read_fds;
    struct timeval timeout;

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    timeout.tv_sec = 0;
    timeout.tv_usec = 1000 * ms; // 100 milliseconds timeout

    int ret = select(fd + 1, &read_fds, NULL, NULL, &timeout);
    return ret > 0 && FD_ISSET(fd, &read_fds);
}

bool rfd_wait_forever(int fd) {
    while ((!rfd_wait(fd, 10))) {
    }
    return true;
}

size_t rfile_size(char *path) {
    struct stat s;
    stat(path, &s);
    return s.st_size;
}

size_t rfile_readb(char *path, void *data, size_t size) {
    FILE *fd = fopen(path, "r");
    if (!fd) {
        return 0;
    }
    size_t bytes_read = fread(data, sizeof(char), size, fd);

    fclose(fd);
    ((char *)data)[bytes_read] = 0;
    return bytes_read;
}

#endif
#include <arpa/inet.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BUFF_SIZE 1337
#define RHTTP_MAX_CONNECTIONS 100

int rhttp_opt_error = 1;
int rhttp_opt_warn = 1;
int rhttp_opt_info = 1;
int rhttp_opt_port = 8080;
int rhttp_opt_debug = 0;
int rhttp_opt_request_logging = 0;
int rhttp_sock = 0;
int rhttp_opt_buffered = 0;
int rhttp_c = 0;
int rhttp_c_mutex_initialized = 0;
pthread_mutex_t rhttp_c_mutex;
char rhttp_opt_host[1024] = "0.0.0.0";
unsigned int rhttp_connections_handled = 0;

typedef struct rhttp_header_t {
    char *name;
    char *value;
    struct rhttp_header_t *next;
} rhttp_header_t;

typedef struct rhttp_request_t {
    int c;
    int closed;
    nsecs_t start;
    char *raw;
    char *line;
    char *body;
    char *method;
    char *path;
    char *version;
    void *context;
    unsigned int bytes_received;
    rhttp_header_t *headers;
} rhttp_request_t;

char *rhttp_current_timestamp() {
    time_t current_time;
    time(&current_time);
    struct tm *local_time = localtime(&current_time);
    static char time_string[100];
    time_string[0] = 0;
    strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", local_time);

    return time_string;
}

void rhttp_logs(const char *prefix, const char *level, const char *format,
                va_list args) {
    char buf[strlen(format) + BUFSIZ + 1];
    buf[0] = 0;
    sprintf(buf, "%s%s %s %s\e[0m", prefix, rhttp_current_timestamp(), level,
            format);
    vfprintf(stdout, buf, args);
}
void rhttp_log_info(const char *format, ...) {
    if (!rhttp_opt_info)
        return;
    va_list args;
    va_start(args, format);
    rhttp_logs("\e[32m", "INFO ", format, args);
    va_end(args);
}
void rhttp_log_debug(const char *format, ...) {
    if (!rhttp_opt_debug)
        return;
    va_list args;
    va_start(args, format);
    if (rhttp_opt_debug)
        rhttp_logs("\e[33m", "DEBUG", format, args);

    va_end(args);
}
void rhttp_log_warn(const char *format, ...) {
    if (!rhttp_opt_warn)
        return;
    va_list args;
    va_start(args, format);
    rhttp_logs("\e[34m", "WARN ", format, args);

    va_end(args);
}
void rhttp_log_error(const char *format, ...) {
    if (!rhttp_opt_error)
        return;
    va_list args;
    va_start(args, format);
    rhttp_logs("\e[35m", "ERROR", format, args);

    va_end(args);
}

void http_request_init(rhttp_request_t *r) {
    r->raw = NULL;
    r->line = NULL;
    r->body = NULL;
    r->method = NULL;
    r->path = NULL;
    r->version = NULL;
    r->start = 0;
    r->headers = NULL;
    r->bytes_received = 0;
    r->closed = 0;
}

void rhttp_free_header(rhttp_header_t *h) {
    if (!h)
        return;
    rhttp_header_t *next = h->next;
    free(h->name);
    free(h->value);
    free(h);
    if (next)
        rhttp_free_header(next);
}
void rhttp_rhttp_free_headers(rhttp_request_t *r) {
    if (!r->headers)
        return;
    rhttp_free_header(r->headers);
    r->headers = NULL;
}

rhttp_header_t *rhttp_parse_headers(rhttp_request_t *s) {
    int first = 1;
    char *body = s->body;
    while (body && *body) {
        char *line = __strtok_r(first ? body : NULL, "\r\n", &body);
        if (!line)
            break;
        rhttp_header_t *h = (rhttp_header_t *)malloc(sizeof(rhttp_header_t));
        h->name = NULL;
        h->value = NULL;
        h->next = NULL;
        char *name = __strtok_r(line, ": ", &line);
        first = 0;
        if (!name) {
            rhttp_free_header(h);
            break;
        }
        h->name = strdup(name);
        char *value = __strtok_r(NULL, "\r\n", &line);
        if (!value) {
            rhttp_free_header(h);
            break;
        }
        h->value = value ? strdup(value + 1) : strdup("");
        h->next = s->headers;
        s->headers = h;
    }
    return s->headers;
}

void rhttp_free_request(rhttp_request_t *r) {
    if (r->bytes_received != 0) {

        free(r->raw);
        free(r->body);

        free(r->method);
        free(r->path);
        free(r->version);
        rhttp_rhttp_free_headers(r);
    }
    free(r);
}

void rhttp_print_header(rhttp_header_t *h) {
    rhttp_log_debug("Header: <%s> \"%s\"\n", h->name, h->value);
}
void rhttp_print_headers(rhttp_header_t *h) {
    while (h) {
        rhttp_print_header(h);
        h = h->next;
    }
}
void rhttp_print_request_line(rhttp_request_t *r) {
    rhttp_log_info("%s %s %s\n", r->method, r->path, r->version);
}
void rhttp_print_request(rhttp_request_t *r) {
    rhttp_print_request_line(r);
    if (rhttp_opt_debug)
        rhttp_print_headers(r->headers);
}
void rhttp_close(rhttp_request_t *r) {
    if (!r)
        return;
    if (!r->closed)
        close(r->c);
    rhttp_free_request(r);
}
rhttp_request_t *rhttp_parse_request(int s) {
    rhttp_request_t *request =
        (rhttp_request_t *)malloc(sizeof(rhttp_request_t));
    http_request_init(request);
    char buf[BUFF_SIZE] = {0};
    request->c = s;
    int breceived = read(s, buf, BUFF_SIZE);
    if (breceived <= 0) {
        close(request->c);
        request->closed = 1;
        return request;
    }
    buf[breceived] = '\0';
    char *original_buf = buf;

    char *b = original_buf;
    request->raw = strdup(b);
    b = original_buf;
    char *line = strtok(b, "\r\n");
    b = original_buf;
    char *body = b + strlen(line) + 2;
    request->body = strdup(body);
    b = original_buf;
    char *method = strtok(b, " ");
    char *path = strtok(NULL, " ");
    char *version = strtok(NULL, " ");
    request->bytes_received = breceived;
    request->line = line;
    request->start = nsecs();
    request->method = strdup(method);
    request->path = strdup(path);
    request->version = strdup(version);
    request->headers = NULL;
    rhttp_parse_headers(request);
    return request;
}

void rhttp_close_server() {
    close(rhttp_sock);
    close(rhttp_c);
    printf("Connections handled: %d\n", rhttp_connections_handled);
    printf("Gracefully closed\n");
    exit(0);
}

size_t rhttp_send_drain(int s, void *tsend, size_t to_send_len) {
    if (to_send_len == 0 && *(unsigned char *)tsend) {
        to_send_len = strlen(tsend);
    }
    unsigned char *to_send = (unsigned char *)malloc(to_send_len);
    unsigned char *to_send_original = to_send;

    memcpy(to_send, tsend, to_send_len);
    // to_send[to_send_len] = '\0';
    size_t bytes_sent = 0;
    size_t bytes_sent_total = 0;
    while (1) {
        bytes_sent = send(s, to_send, to_send_len, 0);
        if (bytes_sent <= 0) {
            bytes_sent_total = 0;
            break;
        }
        bytes_sent_total += bytes_sent;
        to_send += bytes_sent;
        if (bytes_sent_total == to_send_len) {
            break;
        } else {
            rhttp_log_info("Extra send of %d bytes.\n", to_send_len);
        }
    }

    free(to_send_original);
    return bytes_sent_total;
}

typedef int (*rhttp_request_handler_t)(rhttp_request_t *r);

void rhttp_serve(const char *host, int port, int backlog, int request_logging,
                 int request_debug, rhttp_request_handler_t handler,
                 void *context) {
    rhttp_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host ? host : "0.0.0.0");
    rhttp_opt_debug = request_debug;
    rhttp_opt_request_logging = request_logging;
    int opt = 1;
    setsockopt(rhttp_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(rhttp_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("Binding error\n");
        exit(1);
    }
    listen(rhttp_sock, backlog);
    while (1) {
        struct sockaddr_in client_addr;
        int addrlen = sizeof(client_addr);

        rhttp_c = accept(rhttp_sock, (struct sockaddr *)&client_addr,
                         (socklen_t *)&addrlen);
        rhttp_connections_handled++;
        rhttp_request_t *r = rhttp_parse_request(rhttp_c);
        r->context = context;
        if (!r->closed) {
            handler(r);
        }
        rhttp_close(r);
    }
}

unsigned int rhttp_calculate_number_char_count(unsigned int number) {
    unsigned int width = 1;
    unsigned int tcounter = number;
    while (tcounter / 10 >= 1) {
        tcounter = tcounter / 10;
        width++;
    }
    return width;
}

int rhttp_file_response(rhttp_request_t *r, char *path) {
    if (!*path)
        return 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return 0;
    size_t file_size = rfile_size(path);
    char response[1024] = {0};
    char content_type_header[100] = {0};
    char *ext = strstr(path, ".");
    char *text_extensions = ".h,.c,.html";
    if (strstr(text_extensions, ext)) {
        sprintf(content_type_header, "Content-Type: %s\r\n", "text/html");
    }
    sprintf(response, "HTTP/1.1 200 OK\r\n%sContent-Length:%ld\r\n\r\n",
            content_type_header, file_size);
    if (!rhttp_send_drain(r->c, response, 0)) {
        rhttp_log_error("Error sending file: %s\n", path);
    }
    size_t bytes = 0;
    size_t bytes_sent = 0;
    unsigned char file_buff[1024];
    while ((bytes = fread(file_buff, sizeof(char), sizeof(file_buff), f))) {
        if (!rhttp_send_drain(r->c, file_buff, bytes)) {
            rhttp_log_error("Error sending file during chunking: %s\n", path);
        }
        bytes_sent += bytes;
    }
    if (bytes_sent != file_size) {
        rhttp_send_drain(r->c, file_buff, file_size - bytes_sent);
    }
    close(r->c);
    fclose(f);
    return 1;
};

int rhttp_file_request_handler(rhttp_request_t *r) {
    char *path = r->path;
    while (*path == '/' || *path == '.')
        path++;
    if (strstr(path, "..")) {
        return 0;
    }
    return rhttp_file_response(r, path);
};

unsigned int counter = 100000000;
int rhttp_counter_request_handler(rhttp_request_t *r) {
    if (!strncmp(r->path, "/counter", strlen("/counter"))) {
        counter++;
        unsigned int width = rhttp_calculate_number_char_count(counter);
        char to_send2[1024] = {0};
        sprintf(to_send2,
                "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nConnection: "
                "close\r\n\r\n%d",
                width, counter);
        rhttp_send_drain(r->c, to_send2, 0);
        close(r->c);
        return 1;
    }
    return 0;
}
int rhttp_root_request_handler(rhttp_request_t *r) {
    if (!strcmp(r->path, "/")) {
        char to_send[1024] = {0};
        sprintf(to_send, "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: "
                         "close\r\n\r\nOk!");
        rhttp_send_drain(r->c, to_send, 0);
        close(r->c);
        return 1;
    }
    return 0;
}
int rhttp_error_404_handler(rhttp_request_t *r) {
    char to_send[1024] = {0};
    sprintf(to_send, "HTTP/1.1 404 Document not found\r\nContent-Length: "
                     "0\r\nConnection: close\r\n\r\n");
    rhttp_send_drain(r->c, to_send, 0);
    close(r->c);
    return 1;
}

int rhttp_default_request_handler(rhttp_request_t *r) {
    if (rhttp_opt_debug || rhttp_opt_request_logging)
        rhttp_print_request(r);
    if (rhttp_counter_request_handler(r)) {
        // Counter handler
        rhttp_log_info("Counter handler found for: %s\n", r->path);

    } else if (rhttp_root_request_handler(r)) {
        // Root handler
        rhttp_log_info("Root handler found for: %s\n", r->path);
    } else if (rhttp_file_request_handler(r)) {
        rhttp_log_info("File %s sent\n", r->path);
    } else if (rhttp_error_404_handler(r)) {
        rhttp_log_warn("Error 404 for: %s\n", r->path);
        // Error handler
    } else {
        rhttp_log_warn("No handler found for: %s\n", r->path);
        close(rhttp_c);
    }
    return 0;
}

int rhttp_main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
    int opt;
    while ((opt = getopt(argc, argv, "p:drh:bewi")) != -1) {
        switch (opt) {
        case 'i':
            rhttp_opt_info = 1;
            rhttp_opt_warn = 1;
            rhttp_opt_error = 1;
            break;
        case 'e':
            rhttp_opt_error = 1;
            rhttp_opt_warn = 0;
            rhttp_opt_info = 0;
            break;
        case 'w':
            rhttp_opt_warn = 1;
            rhttp_opt_error = 1;
            rhttp_opt_info = 0;
            break;
        case 'p':
            rhttp_opt_port = atoi(optarg);
            break;
        case 'b':
            rhttp_opt_buffered = 1;
            printf("Logging is buffered. Output may be incomplete.\n");
            break;
        case 'h':
            strcpy(rhttp_opt_host, optarg);
            break;
        case 'd':
            printf("Debug enabled\n");
            rhttp_opt_debug = 1;
            rhttp_opt_warn = 1;
            rhttp_opt_info = 1;
            rhttp_opt_error = 1;
            break;
        case 'r':
            printf("Request logging enabled\n");
            rhttp_opt_request_logging = 1;
            break;
        default:
            printf("Usage: %s [-p port] [-h host] [-b]\n", argv[0]);
            return 1;
        }
    }

    printf("Starting server on: %s:%d\n", rhttp_opt_host, rhttp_opt_port);
    if (rhttp_opt_buffered)
        setvbuf(stdout, NULL, _IOFBF, BUFSIZ);

    rhttp_serve(rhttp_opt_host, rhttp_opt_port, 1024, rhttp_opt_request_logging,
                rhttp_opt_debug, rhttp_default_request_handler, NULL);

    return 0;
}

/* CLIENT CODE */

typedef struct rhttp_client_request_t {
    char *host;
    int port;
    char *path;
    bool is_done;
    char *request;
    char *response;
    pthread_t thread;
    int bytes_received;
} rhttp_client_request_t;

rhttp_client_request_t *rhttp_create_request(const char *host, int port,
                                             const char *path) {
    rhttp_client_request_t *r =
        (rhttp_client_request_t *)malloc(sizeof(rhttp_client_request_t));
    char request_line[4096] = {0};
    sprintf(request_line,
            "GET %s HTTP/1.1\r\n"
            "Host: localhost:8000\r\n"
            "Connection: close\r\n"
            "Accept: */*\r\n"
            "User-Agent: mhttpc\r\n"
            "Accept-Language: en-US,en;q=0.5\r\n"
            "Accept-Encoding: gzip, deflate\r\n"
            "\r\n",
            path);
    r->request = strdup(request_line);
    r->host = strdup(host);
    r->port = port;
    r->path = strdup(path);
    r->is_done = false;
    r->response = NULL;
    r->bytes_received = 0;
    return r;
}

int rhttp_execute_request(rhttp_client_request_t *r) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(r->port);
    addr.sin_addr.s_addr = inet_addr(r->host);

    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        return 0;
    }

    send(s, r->request, strlen(r->request), 0);
    char buf[1024 * 1024] = {0};
    int ret = recv(s, buf, 1024 * 1024, 0);
    if (ret > 0) {
        r->response = strdup(buf);
    }

    close(s);
    return ret;
}
void rhttp_reset_request(rhttp_client_request_t *r) {
    free(r->response);
    r->is_done = false;
    r->response = NULL;
    r->bytes_received = 0;
}
void rhttp_free_client_request(rhttp_client_request_t *r) {
    if (r->request)
        free(r->request);
    if (r->response)
        free(r->response);
    if (r->host)
        free(r->host);
    if (r->path)
        free(r->path);
    free(r);
}

void rhttp_client_bench(int workers, int times, const char *host, int port,
                        const char *path) {
    rhttp_client_request_t *requests[workers];
    while (times > 0) {

        for (int i = 0; i < workers && times; i++) {
            requests[i] = rhttp_create_request(host, port, path);
            rhttp_execute_request(requests[i]);
            times--;
        }
    }
}
char *rhttp_client_get(const char *host, int port, const char *path) {
    if (!rhttp_c_mutex_initialized) {
        rhttp_c_mutex_initialized = 1;
        pthread_mutex_init(&rhttp_c_mutex, NULL);
    }
    char http_response[1024 * 1024];
    http_response[0] = 0;
    rhttp_client_request_t *r = rhttp_create_request(host, port, path);
    unsigned int reconnects = 0;
    unsigned int reconnects_max = 100000;
    while (!rhttp_execute_request(r)) {
        reconnects++;
        tick();
        if (reconnects == reconnects_max) {
            fprintf(stderr, "Maxium reconnects exceeded for %s:%d\n", host,
                    port);
            rhttp_free_client_request(r);
            return NULL;
        }
    }
    r->is_done = true;
    char *body = r->response ? strstr(r->response, "\r\n\r\n") : NULL;
    pthread_mutex_lock(&rhttp_c_mutex);
    if (body) {
        strcpy(http_response, body + 4);
    } else {
        strcpy(http_response, r->response);
    }
    rhttp_free_client_request(r);
    char *result = sbuf(http_response);
    pthread_mutex_unlock(&rhttp_c_mutex);
    return result;
}
/*END CLIENT CODE */
#endif

#ifndef RJSON_H
#define RJSON_H

typedef struct rjson_t {
    char *content;
    size_t length;
    size_t size;
} rjson_t;

rjson_t *rjson() {
    rjson_t *json = rmalloc(sizeof(rjson_t));
    json->size = 1024;
    json->length = 0;
    json->content = (char *)rmalloc(json->size);
    json->content[0] = 0;
    return json;
}

void rjson_write(rjson_t *rjs, char *content) {
    size_t len = strlen(content);
    while (rjs->size < rjs->length + len + 1) {
        rjs->content = realloc(rjs->content, rjs->size + 1024);
        rjs->size += 1024;
    }
    strcat(rjs->content, content);
    rjs->length += len;
}

void rjson_object_start(rjson_t *rjs) {
    if (rstrendswith(rjs->content, "}"))
        rjson_write(rjs, ",");
    rjson_write(rjs, "{");
}
void rjson_object_close(rjson_t *rjs) {
    if (rstrendswith(rjs->content, ",")) {
        rjs->content[rjs->length - 1] = 0;
        rjs->length--;
    }
    rjson_write(rjs, "}");
}
void rjson_array_start(rjson_t *rjs) {
    if (rjs->length &&
        (rstrendswith(rjs->content, "}") || rstrendswith(rjs->content, "]")))
        rjson_write(rjs, ",");
    rjson_write(rjs, "[");
}
void rjson_array_close(rjson_t *rjs) {
    if (rstrendswith(rjs->content, ",")) {
        rjs->content[rjs->length - 1] = 0;
        rjs->length--;
    }
    rjson_write(rjs, "]");
}

void rjson_kv_string(rjson_t *rjs, char *key, char *value) {
    if (rjs->length && !rstrendswith(rjs->content, "{") &&
        !rstrendswith(rjs->content, "[")) {
        rjson_write(rjs, ",");
    }
    rjson_write(rjs, "\"");
    rjson_write(rjs, key);
    rjson_write(rjs, "\":\"");
    char *value_str = (char *)rmalloc(strlen(value) + 4096);
    rstraddslashes(value, value_str);
    rjson_write(rjs, value_str);
    free(value_str);
    rjson_write(rjs, "\"");
}

void rjson_kv_int(rjson_t *rjs, char *key, ulonglong value) {
    if (rjs->length && !rstrendswith(rjs->content, "{") &&
        !rstrendswith(rjs->content, "[")) {
        rjson_write(rjs, ",");
    }
    rjson_write(rjs, "\"");
    rjson_write(rjs, key);
    rjson_write(rjs, "\":");
    char value_str[100] = {0};
    sprintf(value_str, "%lld", value);
    rjson_write(rjs, value_str);
}
void rjson_kv_number(rjson_t *rjs, char *key, ulonglong value) {
    if (rjs->length && !rstrendswith(rjs->content, "{") &&
        !rstrendswith(rjs->content, "[")) {
        rjson_write(rjs, ",");
    }
    rjson_write(rjs, "\"");
    rjson_write(rjs, key);
    rjson_write(rjs, "\":");
    rjson_write(rjs, "\"");

    rjson_write(rjs, sbuf(rformat_number(value)));
    rjson_write(rjs, "\"");
}

void rjson_kv_bool(rjson_t *rjs, char *key, int value) {
    if (rjs->length && !rstrendswith(rjs->content, "{") &&
        !rstrendswith(rjs->content, "[")) {
        rjson_write(rjs, ",");
    }
    rjson_write(rjs, "\"");
    rjson_write(rjs, key);
    rjson_write(rjs, "\":");
    rjson_write(rjs, value > 0 ? "true" : "false");
}

void rjson_kv_duration(rjson_t *rjs, char *key, nsecs_t value) {
    if (rjs->length && !rstrendswith(rjs->content, "{") &&
        !rstrendswith(rjs->content, "[")) {
        rjson_write(rjs, ",");
    }
    rjson_write(rjs, "\"");
    rjson_write(rjs, key);
    rjson_write(rjs, "\":");
    rjson_write(rjs, "\"");

    rjson_write(rjs, sbuf(format_time(value)));
    rjson_write(rjs, "\"");
}
void rjson_free(rjson_t *rsj) {
    free(rsj->content);
    free(rsj);
}

void rjson_key(rjson_t *rsj, char *key) {
    rjson_write(rsj, "\"");
    rjson_write(rsj, key);
    rjson_write(rsj, "\":");
}
#endif
#ifndef RAUTOCOMPLETE_H
#define RAUTOCOMPLETE_H
#define R4_DEBUG
#ifndef RREX4_H
#define RREX4_H
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define R4_DEBUG_a

#ifdef R4_DEBUG
static int _r4_debug = 1;
#else
static int _r4_debug = 0;
#endif

static char *_format_function_name(const char *name) {
    static char result[100];
    result[0] = 0;

    char *new_name = (char *)name;
    new_name += 11;
    if (new_name[0] == '_')
        new_name += 1;
    if (strlen(new_name) == 0) {
        return " -";
    }
    strcpy(result, new_name);
    return result;
}

#define DEBUG_VALIDATE_FUNCTION                                                \
    if (_r4_debug || r4->debug)                                                \
        printf("DEBUG: %s %s <%s> \"%s\"\n", _format_function_name(__func__),  \
               r4->valid ? "valid" : "INVALID", r4->expr, r4->str);

struct r4_t;

void r4_enable_debug() { _r4_debug = true; }
void r4_disable_debug() { _r4_debug = false; }

typedef bool (*r4_function)(struct r4_t *);

typedef struct r4_t {
    bool debug;
    bool valid;
    bool in_block;
    bool is_greedy;
    bool in_range;
    unsigned int backtracking;
    unsigned int loop_count;
    unsigned int in_group;
    unsigned int match_count;
    unsigned int validation_count;
    unsigned int start;
    unsigned int end;
    unsigned int length;
    bool (*functions[254])(struct r4_t *);
    bool (*slash_functions[254])(struct r4_t *);
    char *_str;
    char *_expr;
    char *match;
    char *str;
    char *expr;
    char *str_previous;
    char *expr_previous;
    char **matches;
} r4_t;

static bool v4_initiated = false;
typedef bool (*v4_function_map)(r4_t *);
v4_function_map v4_function_map_global[256];
v4_function_map v4_function_map_slash[256];
v4_function_map v4_function_map_block[256];

void r4_free_matches(r4_t *r) {
    if (!r)
        return;
    if (r->match) {
        free(r->match);
        r->match = NULL;
    }
    if (!r->match_count) {
        return;
    }
    for (unsigned i = 0; i < r->match_count; i++) {
        free(r->matches[i]);
    }
    free(r->matches);
    r->match_count = 0;
    r->matches = NULL;
}

void r4_free(r4_t *r) {
    if (!r)
        return;
    r4_free_matches(r);
    free(r);
}

static bool r4_backtrack(r4_t *r4);
static bool r4_validate(r4_t *r4);
static void r4_match_add(r4_t *r4, char *extracted);

static bool r4_validate_literal(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    if (!r4->valid)
        return false;
    if (*r4->str != *r4->expr) {
        r4->valid = false;
    } else {
        r4->str++;
    }
    r4->expr++;
    if (r4->in_block || r4->in_range || !r4->is_greedy) {
        return r4->valid;
    }
    return r4_validate(r4);
}
static bool r4_validate_question_mark(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    r4->valid = true;
    r4->expr++;
    return r4_validate(r4);
}

static bool r4_validate_plus(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    r4->expr++;
    if (r4->valid == false) {
        return r4_validate(r4);
    }
    char *expr_left = r4->expr_previous;
    char *expr_right = r4->expr;
    char *str = r4->str;
    char *return_expr = NULL;
    if (*expr_right == ')') {
        return_expr = expr_right;
        expr_right++;
    }
    r4->is_greedy = false;
    r4->expr = expr_left;
    while (r4->valid) {
        if (*expr_right) {
            r4->expr = expr_right;
            r4->is_greedy = true;
            if (r4_backtrack(r4)) {

                if (return_expr) {
                    r4->str = str;
                    r4->expr = return_expr;
                }
                return r4_validate(r4);
            } else {
                r4->is_greedy = false;
            }
        }
        r4->valid = true;
        r4->expr = expr_left;
        r4->str = str;
        r4_validate(r4);
        str = r4->str;
    }
    r4->is_greedy = true;
    r4->valid = true;
    r4->expr = return_expr ? return_expr : expr_right;
    return r4_validate(r4);
}

static bool r4_validate_dollar(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    r4->expr++;
    r4->valid = *r4->str == 0;
    return r4_validate(r4);
}

static bool r4_validate_roof(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    if (r4->str != r4->_str) {
        return false;
    }
    r4->expr++;
    return r4_validate(r4);
}

static bool r4_validate_dot(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    if (*r4->str == 0) {
        return false;
    }
    r4->expr++;
    r4->valid = *r4->str != '\n';
    r4->str++;

    if (r4->in_block || r4->in_range || !r4->is_greedy) {
        return r4->valid;
    }
    return r4_validate(r4);
}

static bool r4_validate_asterisk(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    r4->expr++;
    if (r4->valid == false) {
        r4->valid = true;
        return r4->valid;
        // return r4_validate(r4);
    }
    char *expr_left = r4->expr_previous;
    char *expr_right = r4->expr;
    char *str = r4->str;
    char *return_expr = NULL;
    if (*expr_right == ')') {
        return_expr = expr_right;
        expr_right++;
    }
    r4->is_greedy = false;
    r4->expr = expr_left;
    while (r4->valid) {
        if (*expr_right) {
            r4->expr = expr_right;
            r4->is_greedy = true;
            if (r4_backtrack(r4)) {

                if (return_expr) {
                    r4->str = str;
                    r4->expr = return_expr;
                }
                return r4_validate(r4);
            } else {
                r4->is_greedy = false;
            }
        }
        r4->valid = true;
        r4->expr = expr_left;
        r4->str = str;
        r4_validate(r4);
        str = r4->str;
    }
    r4->is_greedy = true;
    r4->valid = true;
    r4->expr = return_expr ? return_expr : expr_right;
    return r4_validate(r4);
}

static bool r4_validate_pipe(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    r4->expr++;
    if (r4->valid == true) {
        return true;
    } else {
        r4->valid = true;
    }
    return r4_validate(r4);
}

static bool r4_validate_digit(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    if (!isdigit(*r4->str)) {
        r4->valid = false;
    } else {
        r4->str++;
    }
    r4->expr++;
    if (r4->in_block || r4->in_range || !r4->is_greedy) {
        return r4->valid;
    }
    return r4_validate(r4);
}
static bool r4_validate_not_digit(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    if (isdigit(*r4->str)) {
        r4->valid = false;
    } else {
        r4->str++;
    }
    r4->expr++;

    if (r4->in_block || r4->in_range || !r4->is_greedy) {
        return r4->valid;
    }
    return r4_validate(r4);
}
static bool r4_validate_word(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    if (!isalpha(*r4->str)) {
        r4->valid = false;
    } else {
        r4->str++;
    }
    r4->expr++;

    if (r4->in_block || r4->in_range || !r4->is_greedy) {
        return r4->valid;
    }
    return r4_validate(r4);
}
static bool r4_validate_not_word(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    if (isalpha(*r4->str)) {
        r4->valid = false;
    } else {
        r4->str++;
    }
    r4->expr++;

    if (r4->in_block || r4->in_range || !r4->is_greedy) {
        return r4->valid;
    }
    return r4_validate(r4);
}

static bool r4_isrange(char *s) {
    if (!isalnum(*s)) {
        return false;
    }
    if (*(s + 1) != '-') {
        return false;
    }
    return isalnum(*(s + 2));
}

static bool r4_validate_block_open(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    if (r4->valid == false) {
        return false;
    }
    char *expr_self = r4->expr;
    r4->expr++;
    bool reversed = *r4->expr == '^';
    if (reversed) {
        r4->expr++;
    }

    bool valid_once = false;
    r4->in_block = true;
    while (*r4->expr != ']') {
        r4->valid = true;
        if (r4_isrange(r4->expr)) {
            char s = *r4->expr;
            char e = *(r4->expr + 2);
            r4->expr += 2;
            if (s > e) {
                char tempc = s;
                s = e;
                e = tempc;
            }
            if (*r4->str >= s && *r4->str <= e) {
                if (!reversed) {
                    r4->str++;
                }
                valid_once = true;
                break;
            } else {
                r4->expr++;
            }
        } else if (r4_validate(r4)) {
            valid_once = true;
            if (reversed)
                r4->str--;
            break;
        }
    }
    char *expr_end = strchr(r4->expr, ']');

    r4->expr = expr_end ? expr_end : r4->expr;
    r4->in_block = false;
    r4->valid = expr_end && (!reversed ? valid_once : !valid_once);
    r4->expr++;
    r4->expr_previous = expr_self;

    if (r4->in_range || !r4->is_greedy) {
        return r4->valid;
    }
    return r4_validate(r4);
}

static bool r4_validate_whitespace(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    r4->valid = strchr("\r\t \n", *r4->str) != NULL;
    r4->expr++;
    if (r4->valid) {
        r4->str++;
    }
    if (r4->in_range || r4->in_block || !r4->is_greedy) {
        return r4->valid;
    }
    return r4_validate(r4);
}
static bool r4_validate_not_whitespace(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    r4->valid = strchr("\r\t \n", *r4->str) == NULL;
    r4->expr++;
    if (r4->valid) {
        r4->str++;
    }
    if (r4->in_range || r4->in_block || !r4->is_greedy) {
        return r4->valid;
    }
    return r4_validate(r4);
}

static bool r4_validate_range(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION;
    if (r4->valid == false) {
        r4->expr++;
        return false;
    }
    char *previous = r4->expr_previous;
    r4->in_range = true;
    r4->expr++;
    unsigned int start = 0;
    while (isdigit(*r4->expr)) {
        start = 10 * start;
        start += *r4->expr - '0';
        r4->expr++;
    }
    if (start != 0)
        start--;

    unsigned int end = 0;
    bool variable_end_range = false;
    if (*r4->expr == ',') {
        r4->expr++;
        if (!isdigit(*r4->expr)) {
            variable_end_range = true;
        }
    }
    while (isdigit(*r4->expr)) {
        end = end * 10;
        end += *r4->expr - '0';
        r4->expr++;
    }
    r4->expr++;

    bool valid = true;
    char *expr_right = r4->expr;
    for (unsigned int i = 0; i < start; i++) {
        r4->expr = previous;
        valid = r4_validate(r4);
        if (!*r4->str)
            break;
        if (!valid) {
            break;
        }
    }
    r4->expr = expr_right;
    r4->in_range = false;
    if (!r4->valid)
        return false;
    return r4_validate(r4);

    for (unsigned int i = start; i < end; i++) {
        r4->expr = previous;
        valid = r4_validate(r4);
        if (!valid) {
            break;
        }
    }

    while (variable_end_range) {
        r4->in_range = false;
        valid = r4_validate(r4);
        r4->in_range = true;
        if (valid) {
            break;
        }
        r4->in_range = true;
        valid = r4_validate(r4);
        r4->in_range = false;
        if (!valid) {
            break;
        }
    }
    r4->valid = valid;

    return r4_validate(r4);
}

static bool r4_validate_group_close(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    return r4->valid;
}

static bool r4_validate_group_open(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    char *expr_previous = r4->expr_previous;
    r4->expr++;
    bool save_match = r4->in_group == 0;
    r4->in_group++;
    char *str_extract_start = r4->str;
    bool valid = r4_validate(r4);

    if (!valid || *r4->expr != ')') {
        // this is a valid case if not everything between () matches
        r4->in_group--;
        if (save_match == false) {
            r4->valid = true;
        }

        // Not direct return? Not sure
        return r4_validate(r4);
    }
    // if(save_match){
    //     r4->match_count++;
    // }
    if (save_match) {
        char *str_extract_end = r4->str;
        unsigned int extracted_length = str_extract_end - str_extract_start;
        // strlen(str_extract_start) - strlen(str_extract_end);
        char *str_extracted =
            (char *)calloc(sizeof(char), extracted_length + 1);
        strncpy(str_extracted, str_extract_start, extracted_length);
        r4_match_add(r4, str_extracted);
    }
    assert(*r4->expr == ')');
    r4->expr++;
    r4->in_group--;
    r4->expr_previous = expr_previous;
    return r4_validate(r4);
}

static bool r4_validate_slash(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    // The handling code for handling slashes is implemented in r4_validate
    char *expr_previous = r4->expr_previous;
    r4->expr++;
    r4_function f = v4_function_map_slash[(int)*r4->expr];
    r4->expr_previous = expr_previous;
    return f(r4);
}

static void r4_match_add(r4_t *r4, char *extracted) {
    r4->matches =
        (char **)realloc(r4->matches, (r4->match_count + 1) * sizeof(char *));
    r4->matches[r4->match_count] = extracted;
    r4->match_count++;
}

static bool r4_validate_word_boundary_start(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    r4->expr++;
    if (!r4->valid) {
        return r4->valid;
    }
    r4->valid =
        isalpha(*r4->str) && (r4->str == r4->_str || !isalpha(*(r4->str - 1)));
    if (r4->in_range || r4->in_block || !r4->is_greedy) {
        return r4->valid;
    }
    return r4_validate(r4);
}
static bool r4_validate_word_boundary_end(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    r4->expr++;
    if (!r4->valid) {
        return r4->valid;
    }
    r4->valid =
        isalpha(*r4->str) && (*(r4->str + 1) == 0 || !isalpha(*(r4->str + 1)));
    if (r4->in_range || r4->in_block || !r4->is_greedy) {
        return r4->valid;
    }
    return r4_validate(r4);
}

static void v4_init_function_maps() {
    if (v4_initiated)
        return;
    v4_initiated = true;
    for (__uint8_t i = 0; i < 255; i++) {
        v4_function_map_global[i] = r4_validate_literal;
        v4_function_map_slash[i] = r4_validate_literal;
        v4_function_map_block[i] = r4_validate_literal;
    }
    v4_function_map_global['*'] = r4_validate_asterisk;
    v4_function_map_global['?'] = r4_validate_question_mark;
    v4_function_map_global['+'] = r4_validate_plus;
    v4_function_map_global['$'] = r4_validate_dollar;
    v4_function_map_global['^'] = r4_validate_roof;
    v4_function_map_global['.'] = r4_validate_dot;
    v4_function_map_global['|'] = r4_validate_pipe;
    v4_function_map_global['\\'] = r4_validate_slash;
    v4_function_map_global['['] = r4_validate_block_open;
    v4_function_map_global['{'] = r4_validate_range;
    v4_function_map_global['('] = r4_validate_group_open;
    v4_function_map_global[')'] = r4_validate_group_close;
    v4_function_map_slash['b'] = r4_validate_word_boundary_start;
    v4_function_map_slash['B'] = r4_validate_word_boundary_end;
    v4_function_map_slash['d'] = r4_validate_digit;
    v4_function_map_slash['w'] = r4_validate_word;
    v4_function_map_slash['D'] = r4_validate_not_digit;
    v4_function_map_slash['W'] = r4_validate_not_word;
    v4_function_map_slash['s'] = r4_validate_whitespace;
    v4_function_map_slash['S'] = r4_validate_not_whitespace;
    v4_function_map_block['\\'] = r4_validate_slash;

    v4_function_map_block['{'] = r4_validate_range;
}

void r4_init(r4_t *r4) {
    v4_init_function_maps();
    if (r4 == NULL)
        return;
    r4->debug = _r4_debug;
    r4->valid = true;
    r4->validation_count = 0;
    r4->match_count = 0;
    r4->start = 0;
    r4->end = 0;
    r4->length = 0;
    r4->matches = NULL;
}

static bool r4_looks_behind(char c) { return strchr("?*+{", c) != NULL; }

r4_t *r4_new() {
    r4_t *r4 = (r4_t *)malloc(sizeof(r4_t));

    r4_init(r4);

    return r4;
}

static bool r4_pipe_next(r4_t *r4) {
    char *expr = r4->expr;
    while (*expr) {
        if (*expr == '|') {
            r4->expr = expr + 1;
            r4->valid = true;
            return true;
        }
        expr++;
    }
    return false;
}

static bool r4_backtrack(r4_t *r4) {
    if (_r4_debug)
        printf("\033[36mDEBUG: backtrack start (%d)\n", r4->backtracking);
    r4->backtracking++;
    char *str = r4->str;
    char *expr = r4->expr;
    bool result = r4_validate(r4);
    r4->backtracking--;
    if (result == false) {
        r4->expr = expr;
        r4->str = str;
    }
    if (_r4_debug)
        printf("DEBUG: backtrack end (%d) result: %d %s\n", r4->backtracking,
               result, r4->backtracking == 0 ? "\033[0m" : "");
    return result;
}

static bool r4_validate(r4_t *r4) {
    DEBUG_VALIDATE_FUNCTION
    r4->validation_count++;
    char c_val = *r4->expr;
    if (c_val == 0) {
        return r4->valid;
    }
    if (!r4_looks_behind(c_val)) {
        r4->expr_previous = r4->expr;
    } else if (r4->expr == r4->_expr) {
        // Regex may not start with a look behind ufnction
        return false;
    }

    if (!r4->valid && !r4_looks_behind(*r4->expr)) {
        if (!r4_pipe_next(r4)) {
            return false;
        }
    }
    r4_function f;
    if (r4->in_block) {
        f = v4_function_map_block[(int)c_val];
    } else {
        f = v4_function_map_global[(int)c_val];
    }

    r4->valid = f(r4);
    return r4->valid;
}

char *r4_get_match(r4_t *r) {
    char *match = (char *)malloc(r->length + 1);
    strncpy(match, r->_str + r->start, r->length);
    match[r->length] = 0;
    return match;
}

static bool r4_search(r4_t *r) {
    bool valid = true;
    char *str_next = r->str;
    while (*r->str) {
        if (!(valid = r4_validate(r))) {
            // Move next until we find a match
            if (!r->backtracking) {
                r->start++;
            }
            str_next++;
            r->str = str_next;
            r->expr = r->_expr;
            r->valid = true;
        } else {
            /// HIGH DOUBT
            if (!r->backtracking) {
                // r->start = 0;
            }
            break;
        }
    }
    r->valid = valid;
    if (r->valid) {
        r->end = strlen(r->_str) - strlen(r->str);
        r->length = r->end - r->start;
        r->match = r4_get_match(r);
    }
    return r->valid;
}

r4_t *r4(const char *str, const char *expr) {
    r4_t *r = r4_new();
    r->_str = (char *)str;
    r->_expr = (char *)expr;
    r->match = NULL;
    r->str = r->_str;
    r->expr = r->_expr;
    r->str_previous = r->_str;
    r->expr_previous = r->expr;
    r->in_block = false;
    r->is_greedy = true;
    r->in_group = 0;
    r->loop_count = 0;
    r->backtracking = 0;
    r->in_range = false;
    r4_search(r);
    return r;
}

r4_t *r4_next(r4_t *r, char *expr) {
    if (expr) {
        r->_expr = expr;
    }
    r->backtracking = 0;
    r->expr = r->_expr;
    r->is_greedy = true;
    r->in_block = false;
    r->in_range = false;
    r->in_group = false;
    r4_free_matches(r);
    r4_search(r);
    return r;
}

bool r4_match(char *str, char *expr) {
    r4_t *r = r4(str, expr);
    bool result = r->valid;
    r4_free(r);
    return result;
}
#endif
#define rautocomplete_new rstring_list_new
#define rautocomplete_free rstring_list_free
#define rautocomplete_add rstring_list_add
#define rautocomplete_find rstring_list_find
#define rautocomplete_t rstring_list_t
#define rautocomplete_contains rstring_list_contains

char *r4_escape(char *content) {
    size_t size = strlen(content) * 2 + 1;
    char *escaped = (char *)calloc(size, sizeof(char));
    char *espr = escaped;
    char *to_escape = "?*+()[]{}^$\\";
    *espr = '(';
    espr++;
    while (*content) {
        if (strchr(to_escape, *content)) {
            *espr = '\\';
            espr++;
        }
        *espr = *content;
        espr++;
        content++;
    }
    *espr = '.';
    espr++;
    *espr = '+';
    espr++;
    *espr = ')';
    espr++;
    *espr = 0;
    return escaped;
}

char *rautocomplete_find(rstring_list_t *list, char *expr) {
    if (!list->count)
        return NULL;
    if (!expr || !strlen(expr))
        return NULL;

    char *escaped = r4_escape(expr);

    for (unsigned int i = list->count - 1; i == 0; i--) {
        char *match;
        r4_t *r = r4(list->strings[i], escaped);
        if (r->valid && r->match_count == 1) {
            match = strdup(r->matches[0]);
        }
        r4_free(r);
        if (match) {

            free(escaped);
            return match;
        }
    }
    free(escaped);
    return NULL;
}
#endif
#ifndef RKEYTABLE_H
#define RKEYTABLE_H
/*
    DERIVED FROM HASH TABLE K&R
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct rnklist {
    struct rnklist *next;
    struct rnklist *last;
    char *name;
    char *defn;
} rnklist;

static rnklist *rkeytab = NULL;

rnklist *rlkget(char *s) {
    rnklist *np;
    for (np = rkeytab; np != NULL; np = np->next)
        if (strcmp(s, np->name) == 0)
            return np; // Found
    return NULL;       // Not found
}

char *rkget(char *s) {
    rnklist *np = rlkget(s);
    return np ? np->defn : NULL;
}

rnklist *rkset(char *name, char *defn) {
    rnklist *np;
    if ((np = (rlkget(name))) == NULL) { // Not found
        np = (rnklist *)malloc(sizeof(rnklist));
        np->name = strdup(name);
        np->next = NULL;
        np->last = NULL;

        if (defn) {
            np->defn = strdup(defn);
        } else {
            np->defn = NULL;
        }

        if (rkeytab == NULL) {
            rkeytab = np;
            rkeytab->last = np;
        } else {
            if (rkeytab->last)
                rkeytab->last->next = np;

            rkeytab->last = np;
        }
    } else {
        if (np->defn)
            free((void *)np->defn);
        if (defn) {
            np->defn = strdup(defn);
        } else {
            np->defn = NULL;
        }
    }
    return np;
}
#endif

#ifndef RHASHTABLE_H
#define RHASHTABLE_H
/*
    ORIGINAL SOURCE IS FROM K&R
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HASHSIZE 101

// Structure for the table entries
typedef struct rnlist {
    struct rnlist *next;
    char *name;
    char *defn;
} rnlist;

// Hash table array
static rnlist *rhashtab[HASHSIZE];

// Hash function
unsigned rhash(char *s) {
    unsigned hashval;
    for (hashval = 0; *s != '\0'; s++)
        hashval = *s + 31 * hashval;
    return hashval % HASHSIZE;
}

rnlist *rlget(char *s) {
    rnlist *np;
    for (np = rhashtab[rhash(s)]; np != NULL; np = np->next)
        if (strcmp(s, np->name) == 0)
            return np; // Found
    return NULL;       // Not found
}

// Lookup function
char *rget(char *s) {
    rnlist *np = rlget(s);
    return np ? np->defn : NULL;
}

// Install function (adds a name and definition to the table)
struct rnlist *rset(char *name, char *defn) {
    struct rnlist *np = NULL;
    unsigned hashval;

    if ((rlget(name)) == NULL) { // Not found
        np = (struct rnlist *)malloc(sizeof(*np));
        if (np == NULL || (np->name = strdup(name)) == NULL)
            return NULL;
        hashval = rhash(name);
        np->next = rhashtab[hashval];
        rhashtab[hashval] = np;
    } else {
        if (np->defn)
            free((void *)np->defn);
        np->defn = NULL;
    }
    if ((np->defn = strdup(defn)) == NULL)
        return NULL;
    return np;
}
#endif

#ifndef RREX3_H
#define RREX3_H
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef RREX3_DEBUG
#define RREX3_DEBUG 0
#endif

struct rrex3_t;

typedef void (*rrex3_function)(struct rrex3_t *);

typedef struct rrex3_t {
    void (*functions[254])(struct rrex3_t *);
    void (*slash_functions[254])(struct rrex3_t *);
    bool valid;
    int match_count;
    int match_capacity;
    char **matches;
    bool exit;
    char *__expr;
    char *__str;
    char *_expr;
    char *_str;
    char *expr;
    char *str;
    char *compiled;
    bool inside_brackets;
    bool inside_parentheses;
    bool pattern_error;
    bool match_from_start;
    char bytecode;
    rrex3_function function;
    struct {
        void (*function)(struct rrex3_t *);
        char *expr;
        char *str;
        char bytecode;
    } previous;
    struct {
        void (*function)(struct rrex3_t *);
        char *expr;
        char *str;
        char bytecode;
    } failed;
} rrex3_t;

static bool isdigitrange(char *s) {
    if (!isdigit(*s)) {
        return false;
    }
    if (*(s + 1) != '-') {
        return false;
    }
    return isdigit(*(s + 2));
}

static bool isalpharange(char *s) {
    if (!isalpha(*s)) {
        return false;
    }
    if (*(s + 1) != '-') {
        return false;
    }
    return isalpha(*(s + 2));
}

void rrex3_free_matches(rrex3_t *rrex3) {
    if (!rrex3->matches)
        return;
    for (int i = 0; i < rrex3->match_count; i++) {
        free(rrex3->matches[i]);
    }
    free(rrex3->matches);
    rrex3->matches = NULL;
    rrex3->match_count = 0;
    rrex3->match_capacity = 0;
}

void rrex3_free(rrex3_t *rrex3) {
    if (!rrex3)
        return;
    if (rrex3->compiled) {
        free(rrex3->compiled);
        rrex3->compiled = NULL;
    }
    rrex3_free_matches(rrex3);
    free(rrex3);
    rrex3 = NULL;
}
static bool rrex3_move(rrex3_t *, bool);
static void rrex3_set_previous(rrex3_t *);
inline static void rrex3_cmp_asterisk(rrex3_t *);
void rrex3_cmp_literal_range(rrex3_t *rrex3) {
#if RREX3_DEBUG == 1
    printf("Range check: %c:%c:%d\n", *rrex3->expr, *rrex3->str, rrex3->valid);
#endif
    rrex3_set_previous(rrex3);

    char start = *rrex3->expr;
    rrex3->expr++;
    rrex3->expr++;
    char end = *rrex3->expr;
    if (*rrex3->str >= start && *rrex3->str <= end) {
        rrex3->str++;
        rrex3->valid = true;
    } else {
        rrex3->valid = false;
    }
    rrex3->expr++;
}

bool rrex3_is_function(char chr) {
    if (chr == ']' || chr == ')' || chr == '\\' || chr == '?' || chr == '+' ||
        chr == '*')
        return true;
    return false;
}

inline static void rrex3_cmp_literal(rrex3_t *rrex3) {
    rrex3_set_previous(rrex3);

    if (rrex3->inside_brackets) {
        if (isalpharange(rrex3->expr) || isdigitrange(rrex3->expr)) {
            rrex3_cmp_literal_range(rrex3);
            return;
        }
    }
#if RREX3_DEBUG == 1
    printf("Literal check: %c:%c:%d\n", *rrex3->expr, *rrex3->str,
           rrex3->valid);

#endif
    if (*rrex3->expr == 0 && !*rrex3->str) {
        printf("ERROR, EMPTY CHECK\n");
        // exit(1);
    }
    if (rrex3->valid == false) {
        rrex3->expr++;
        return;
    }

    if (*rrex3->expr == *rrex3->str) {
        rrex3->expr++;
        rrex3->str++;
        rrex3->valid = true;
        // if(*rrex3->expr &&rrex3->functions[(int)*rrex3->expr] ==
        // rrex3_cmp_literal && !rrex3->inside_brackets &&
        //! rrex3_is_function(*rrex3->expr)){ rrex3_cmp_literal(rrex3);
        //   if(rrex3->valid == false){
        //  rrex3->expr--;
        // rrex3->valid = true;
        // }
        // }
        return;
    }
    rrex3->expr++;
    rrex3->valid = false;
}

inline static void rrex3_cmp_dot(rrex3_t *rrex3) {
#if RREX3_DEBUG == 1
    printf("Dot check (any char): %c:%c:%d\n", *rrex3->expr, *rrex3->str,
           rrex3->valid);
#endif
    rrex3_set_previous(rrex3);
    rrex3->expr++;
    if (!rrex3->valid) {
        return;
    }
    if (*rrex3->str && *rrex3->str != '\n') {
        rrex3->str++;
        if (*rrex3->expr && *rrex3->expr == '.') {
            rrex3_cmp_dot(rrex3);
            return;
        } /*else if(*rrex3->expr && (*rrex3->expr == '*' || *rrex3->expr ==
         '+')){ char * next = strchr(rrex3->str,*(rrex3->expr + 1)); char *
         space = strchr(rrex3->str,'\n'); if(next && (!space || space > next)){
                 rrex3->str = next;
             }
         }*/
    } else {
        rrex3->valid = false;
    }
}

inline static void rrex3_cmp_question_mark(rrex3_t *rrex3) {
#if RREX3_DEBUG == 1
    printf("Question mark check: %c:%c:%d\n", *rrex3->expr, *rrex3->str,
           rrex3->valid);
#endif
    rrex3_set_previous(rrex3);

    if (rrex3->valid == false)
        rrex3->valid = true;
    rrex3->expr++;
}

inline static void rrex3_cmp_whitespace(rrex3_t *rrex3) {
#if RREX3_DEBUG == 1
    printf("Whitespace check: %c:%c:%d\n", *rrex3->expr, *rrex3->str,
           rrex3->valid);
#endif
    rrex3_set_previous(rrex3);

    char c = *rrex3->expr;
    rrex3->valid = c == ' ' || c == '\n' || c == '\t';
    if (rrex3->valid) {
        rrex3->str++;
    }
    rrex3->expr++;
}

inline static void rrex3_cmp_whitespace_upper(rrex3_t *rrex3) {
#if RREX3_DEBUG == 1
    printf("Non whitespace check: %c:%c:%d\n", *rrex3->expr, *rrex3->str,
           rrex3->valid);
#endif
    rrex3_set_previous(rrex3);

    char c = *rrex3->expr;
    rrex3->valid = !(c == ' ' || c == '\n' || c == '\t');
    if (rrex3->valid) {
        rrex3->str++;
    }
    rrex3->expr++;
}

inline static void rrex3_cmp_plus2(rrex3_t *rrex3) {
#if RREX3_DEBUG == 1
    printf("Plus check: %c:%c:%d\n", *rrex3->expr, *rrex3->str, rrex3->valid);
#endif
    rrex3_set_previous(rrex3);

    if (rrex3->valid) {
        rrex3->str--;
    } else {
        return;
    }
    char *original_expr = rrex3->expr;
    char *next = original_expr + 1;
    char *loop_expr = rrex3->previous.expr - 1;
    if (*loop_expr == '+') {
        rrex3->valid = false;
        rrex3->pattern_error = true;
        rrex3->expr++;
        return;
    }
    bool success_next = false;
    bool success_next_once = false;
    bool success_current = false;
    char *next_next = NULL;
    char *next_str = rrex3->str;
    while (*rrex3->str) {
        // Check if next matches
        char *original_str = rrex3->str;
        rrex3->expr = next;
        rrex3->valid = true;
        if (rrex3_move(rrex3, false)) {
            success_next = true;
            next_next = rrex3->expr;
            next_str = rrex3->str;
            success_next_once = true;
        } else {
            success_next = false;
        }
        if (success_next_once && !success_next) {
            break;
        }
        // Check if current matches
        rrex3->str = original_str;
        rrex3->expr = loop_expr;
        rrex3->valid = true;
        if (!*rrex3->str || !rrex3_move(rrex3, false)) {
            success_current = false;
        } else {
            success_current = true;
            if (!success_next) {
                next_next = rrex3->expr + 1; // +1 is the * itself
                next_str = rrex3->str;
            }
        }
        if (success_next && !success_current) {
            break;
        }
    }
    if (!next_next)
        rrex3->expr = next;
    else {
        rrex3->expr = next_next;
    }
    rrex3->str = next_str;
    rrex3->valid = true;
}

inline static void rrex3_cmp_plus(rrex3_t *rrex3) {
#if RREX3_DEBUG == 1
    rprintg("Asterisk start check: %c:%c:%d\n", *rrex3->expr, *rrex3->str,
            rrex3->valid);
#endif
    if (!rrex3->valid) {
        rrex3->expr++;
        return;
    }

    char *left = rrex3->previous.expr;
    // printf("%s\n",rrex3->str);
    char *right = rrex3->expr + 1;
    if (*right == ')') {
        right++;
    }
    int right_valid = 0;
    bool right_valid_once = false;
    char *expr = right;
    char *right_str = rrex3->str;
    ;
    char *right_expr = NULL;
    char *str = rrex3->str;
    bool first_time = true;
    bool left_valid = true;
    char *str_prev = NULL;
    bool valid_from_start = true;
    ;
    while (*rrex3->str) {
        if (!left_valid && !right_valid) {
            break;
        }
        if (right_valid && !left_valid) {
            str = right_str;
            break;
        }

        rrex3->expr = right;
        rrex3->str = str;
#if RREX3_DEBUG == 1
        printf("r");
#endif
        if (*rrex3->str && rrex3_move(rrex3, false)) {
            right_valid++;
            right_str = rrex3->str;
            expr = rrex3->expr;
            if (!right_valid_once) {
                right_expr = rrex3->expr;
                right_valid_once = true;
            }
        } else {
            right_valid = 0;
        }
        if (first_time) {
            first_time = false;
            valid_from_start = right_valid;
        }

        if (right_valid && !valid_from_start && right_valid > 0) {
            expr = right_expr - 1;
            ;
            if (*(right - 1) == ')') {
                expr = right - 1;
            }
            break;
        }

        if ((!right_valid && right_valid_once)) {
            expr = right_expr;
            if (*(right - 1) == ')') {
                str = str_prev;
                expr = right - 1;
            }
            break;
        }

        str_prev = str;
        rrex3->valid = true;
        rrex3->str = str;
        rrex3->expr = left;
#if RREX3_DEBUG == 1
        printf("l");
#endif
        if (rrex3_move(rrex3, false)) {
            left_valid = true;

            str = rrex3->str;
        } else {
            left_valid = false;
        }
    }

    rrex3->expr = expr;
    rrex3->str = str;
    rrex3->valid = true;

#if RREX3_DEBUG == 1
    rprintg("Asterisk end check: %c:%c:%d\n", *rrex3->expr, *rrex3->str,
            rrex3->valid);
#endif
}

inline static void rrex3_cmp_asterisk(rrex3_t *rrex3) {
#if RREX3_DEBUG == 1
    rprintg("Asterisk start check: %c:%c:%d\n", *rrex3->expr, *rrex3->str,
            rrex3->valid);
#endif
    if (!rrex3->valid) {
        rrex3->valid = true;
        rrex3->expr++;
        return;
    }

    rrex3->str = rrex3->previous.str;
    char *left = rrex3->previous.expr;
    // printf("%s\n",rrex3->str);
    char *right = rrex3->expr + 1;
    if (*right == ')') {
        right++;
    }
    int right_valid = 0;
    bool right_valid_once = false;
    char *expr = right;
    char *right_str = rrex3->str;
    ;
    char *right_expr = NULL;
    char *str = rrex3->str;
    bool first_time = true;
    bool left_valid = true;
    char *str_prev = NULL;
    bool valid_from_start = true;
    ;
    while (*rrex3->str) {
        if (!left_valid && !right_valid) {
            break;
        }
        if (right_valid && !left_valid) {
            str = right_str;
            break;
        }

        rrex3->expr = right;
        rrex3->str = str;
#if RREX3_DEBUG == 1
        printf("r");
#endif
        if (*rrex3->str && rrex3_move(rrex3, false)) {
            right_valid++;
            right_str = rrex3->str;
            expr = rrex3->expr;
            if (!right_valid_once) {
                right_expr = rrex3->expr;
                right_valid_once = true;
            }
        } else {
            right_valid = 0;
        }
        if (first_time) {
            first_time = false;
            valid_from_start = right_valid;
        }

        if (right_valid && !valid_from_start && right_valid > 0) {
            expr = right_expr - 1;
            if (*(right - 1) == ')') {
                expr = right - 1;
            }
            break;
        }

        if ((!right_valid && right_valid_once)) {
            expr = right_expr;
            if (*(right - 1) == ')') {
                str = str_prev;
                expr = right - 1;
            }
            break;
        }

        str_prev = str;
        rrex3->valid = true;
        rrex3->str = str;
        rrex3->expr = left;
#if RREX3_DEBUG == 1
        printf("l");
#endif
        if (rrex3_move(rrex3, false)) {
            left_valid = true;
            str = rrex3->str;
        } else {
            left_valid = false;
        }
    }

    rrex3->expr = expr;
    rrex3->str = str;
    rrex3->valid = true;

#if RREX3_DEBUG == 1
    rprintg("Asterisk end check: %c:%c:%d\n", *rrex3->expr, *rrex3->str,
            rrex3->valid);
#endif
}

inline static void rrex3_cmp_asterisk2(rrex3_t *rrex3) {
#if RREX3_DEBUG == 1
    rprintg("Asterisk start check: %c:%c:%d\n", *rrex3->expr, *rrex3->str,
            rrex3->valid);
#endif
    if (!rrex3->valid) {
        rrex3->valid = true;
        rrex3->expr++;
        return;
    }
    if (*rrex3->previous.expr == '*') {
        // Support for **
        rrex3->valid = false;
        // rrex3->pattern_error = true;
        rrex3->expr++;
        return;
    }
    rrex3->str = rrex3->previous.str;
    ;
    char *next = rrex3->expr + 1;
    char *next_original = NULL;
    if (*next == '*') {
        next++;
    }
    if (*next == ')' && *(next + 1)) {
        next_original = next;
        next++;
    }
    char *loop_expr = rrex3->previous.expr;
    bool success_next = false;
    bool success_next_once = false;
    bool success_current = false;
    char *right_next = NULL;
    char *right_str = rrex3->str;
    while (*rrex3->str && *rrex3->expr && *rrex3->expr != ')') {
        // Remember original_str because it's modified
        // by checking right and should be restored
        // for checking left so they're matching the
        // same value.
        char *original_str = rrex3->str;
        // Check if right matches.
        // if(*next != ')'){
        rrex3->expr = next;
        rrex3->valid = true;
        if (rrex3_move(rrex3, false)) {
            // Match rright.
            success_next = true;
            if (!next_original) {
                if (!success_next_once) {
                    right_next = rrex3->expr;
                }

            } else {
                right_next = next_original;
                break;
            }
            right_str = rrex3->str;
            success_next_once = true;
        } else {
            // No match Right.
            success_next = false;
        }
        //}
        if (success_next_once && !success_next) {
            // Matched previous time but now doesn't.
            break;
        }
        // Check if left matches.
        rrex3->str = original_str;
        rrex3->expr = loop_expr;
        rrex3->valid = true;
        if (!rrex3_move(rrex3, false)) {
            // No match left.
            success_current = false;
        } else {
            // Match left.
            success_current = true;
            // NOT SURE< WITHOUT DOET HETZELFDE:
            // original_str = rrex3->str;
            if (!success_next) {
                right_str = rrex3->str;
                if (*rrex3->expr != ')') {
                    right_next = rrex3->expr + 1; // +1 is the * itself

                } else {

                    // break;
                }
            }
        }

        if ((success_next && !success_current) ||
            (!success_next && !success_current)) {
            break;
        }
    }
    rrex3->expr = right_next;
    rrex3->str = right_str;
    rrex3->valid = true;
#if RREX3_DEBUG == 1
    rprintg("Asterisk end check: %c:%c:%d\n", *rrex3->expr, *rrex3->str,
            rrex3->valid);
#endif
}

inline static void rrex3_cmp_roof(rrex3_t *rrex3) {
    rrex3_set_previous(rrex3);
#if RREX3_DEBUG == 1
    printf("<Roof check: %c:%c:%d\n", *rrex3->expr, *rrex3->str, rrex3->valid);
#endif
    rrex3->valid = rrex3->str == rrex3->_str;
    rrex3->match_from_start = true;
    rrex3->expr++;
}
inline static void rrex3_cmp_dollar(rrex3_t *rrex3) {
    rrex3_set_previous(rrex3);

#if RREX3_DEBUG == 1
    printf("Dollar check: %c:%c:%d\n", *rrex3->expr, *rrex3->str, rrex3->valid);
#endif
    if (*rrex3->str || !rrex3->valid) {
        rrex3->valid = false;
    }
    rrex3->expr++;
}

inline static void rrex3_cmp_w(rrex3_t *rrex3) {
    rrex3_set_previous(rrex3);

    rrex3->expr++;
#if RREX3_DEBUG == 1
    printf("Word check: %c:%c:%d\n", *rrex3->expr, *rrex3->str, rrex3->valid);
#endif
    if (isalpha(*rrex3->str)) {
        rrex3->str++;
    } else {
        rrex3->valid = false;
    }
}
inline static void rrex3_cmp_w_upper(rrex3_t *rrex3) {
    rrex3_set_previous(rrex3);

    rrex3->expr++;
#if RREX3_DEBUG == 1
    printf("!Word check: %c:%c:%d\n", *rrex3->expr, *rrex3->str, rrex3->valid);
#endif
    if (!isalpha(*rrex3->str)) {
        rrex3->str++;
    } else {
        rrex3->valid = false;
    }
}

inline static void rrex3_cmp_d(rrex3_t *rrex3) {

    rrex3_set_previous(rrex3);

    rrex3->expr++;
#if RREX3_DEBUG == 1
    printf("Digit check: %c:%c:%d\n", *rrex3->expr, *rrex3->str, rrex3->valid);
#endif
    if (isdigit(*rrex3->str)) {
        rrex3->str++;
    } else {
        rrex3->valid = false;
    }
}
inline static void rrex3_cmp_d_upper(rrex3_t *rrex3) {
    rrex3_set_previous(rrex3);

    rrex3->expr++;
#if RREX3_DEBUG == 1
    printf("!Digit check: %c:%c:%d\n", *rrex3->expr, *rrex3->str, rrex3->valid);
#endif
    if (!isdigit(*rrex3->str)) {
        rrex3->str++;
    } else {
        rrex3->valid = false;
    }
}

inline static void rrex3_cmp_slash(rrex3_t *rrex3) {
    rrex3_set_previous(rrex3);

    rrex3->expr++;

    rrex3->bytecode = *rrex3->expr;
    rrex3->function = rrex3->slash_functions[(int)rrex3->bytecode];
    rrex3->function(rrex3);
}

inline static int collect_digits(rrex3_t *rrex3) {
    char output[20];
    unsigned int digit_count = 0;
    while (isdigit(*rrex3->expr)) {

        output[digit_count] = *rrex3->expr;
        rrex3->expr++;
        digit_count++;
    }
    output[digit_count] = 0;
    return atoi(output);
}

inline static void rrex3_cmp_range(rrex3_t *rrex3) {
    char *loop_code = rrex3->previous.expr;
    char *expr_original = rrex3->expr;
    rrex3->expr++;
    int range_start = collect_digits(rrex3) - 1;
    int range_end = 0;
    if (*rrex3->expr == ',') {
        rrex3->expr++;
        range_end = collect_digits(rrex3);
    }
    rrex3->expr++;
    int times_valid = 0;
    while (*rrex3->str) {
        rrex3->expr = loop_code;
        rrex3_move(rrex3, false);
        if (rrex3->valid == false) {
            break;
        } else {
            times_valid++;
        }
        if (range_end) {
            if (times_valid >= range_start && times_valid == range_end - 1) {
                rrex3->valid = true;
            } else {
                rrex3->valid = false;
            }
            break;
        } else if (range_start) {
            if (times_valid == range_start) {
                rrex3->valid = true;
                break;
            }
        }
    }
    rrex3->valid = times_valid >= range_start;
    if (rrex3->valid && range_end) {
        rrex3->valid = times_valid <= range_end;
    }
    rrex3->expr = strchr(expr_original, '}') + 1;
}

inline static void rrex3_cmp_word_start_or_end(rrex3_t *rrex3) {
#if RREX3_DEBUG == 1
    if (*rrex3->expr != 'B') {
        printf("Check word start or end: %c:%c:%d\n", *rrex3->expr, *rrex3->str,
               rrex3->valid);
    }

#endif
    rrex3_set_previous(rrex3);
    bool valid = false;
    if (isalpha(*rrex3->str)) {
        if (rrex3->_str != rrex3->str) {
            if (!isalpha(*(rrex3->str - 1))) {
                valid = true;
            }
        } else {
            valid = true;
        }
    } else if (isalpha(isalpha(*rrex3->str) && !isalpha(*rrex3->str + 1))) {
        valid = true;
    }
    rrex3->expr++;
    rrex3->valid = valid;
}
inline static void rrex3_cmp_word_not_start_or_end(rrex3_t *rrex3) {
#if RREX3_DEBUG == 1
    printf("Check word NOT start or end: %c:%c:%d\n", *rrex3->expr, *rrex3->str,
           rrex3->valid);

#endif
    rrex3_set_previous(rrex3);

    rrex3_cmp_word_start_or_end(rrex3);
    rrex3->valid = !rrex3->valid;
}

inline static void rrex3_cmp_brackets(rrex3_t *rrex3) {
#if RREX3_DEBUG == 1
    rprintb("\\l Brackets start: %c:%c:%d\n", *rrex3->expr, *rrex3->str,
            rrex3->valid);
#endif
    rrex3_set_previous(rrex3);
    char *original_expr = rrex3->expr;
    rrex3->expr++;
    rrex3->inside_brackets = true;
    bool valid_once = false;
    bool reversed = false;
    if (*rrex3->expr == '^') {
        reversed = true;
        rrex3->expr++;
    }
    bool valid = false;
    while (*rrex3->expr != ']' && *rrex3->expr != 0) {
        rrex3->valid = true;
        valid = rrex3_move(rrex3, false);
        if (reversed) {
            valid = !valid;
        }
        if (valid) {
            valid_once = true;
            if (!reversed) {
                valid_once = true;
                break;
            }
        } else {
            if (reversed) {
                valid_once = false;
                break;
            }
        }
    }
    if (valid_once && reversed) {
        rrex3->str++;
    }
    while (*rrex3->expr != ']' && *rrex3->expr != 0)
        rrex3->expr++;
    if (*rrex3->expr != 0)
        rrex3->expr++;

    rrex3->valid = valid_once;
    rrex3->inside_brackets = false;
    char *previous_expr = rrex3->expr;
    rrex3->expr = original_expr;
    rrex3_set_previous(rrex3);
    rrex3->expr = previous_expr;
#if RREX3_DEBUG == 1
    rprintb("\\l Brackets end: %c:%c:%d\n", *rrex3->expr, *rrex3->str,
            rrex3->valid);
#endif
}

inline static void rrex3_cmp_pipe(rrex3_t *rrex3) {
    rrex3_set_previous(rrex3);

#if RREX3_DEBUG == 1
    printf("Pipe check: %c:%c:%d\n", *rrex3->expr, *rrex3->str, rrex3->valid);
#endif
    if (rrex3->valid == true) {
        rrex3->exit = true;
    } else {
        rrex3->valid = true;
    }
    rrex3->expr++;
}
inline static void rrex3_cmp_parentheses(rrex3_t *rrex3) {
#if RREX3_DEBUG == 1
    rprinty("\\l Parentheses start check: %c:%c:%d\n", *rrex3->expr,
            *rrex3->str, rrex3->valid);
#endif

    rrex3_set_previous(rrex3);
    if (!rrex3->valid) {
        rrex3->expr++;
        return;
    }
    if (rrex3->match_count == rrex3->match_capacity) {

        rrex3->match_capacity++;
        rrex3->matches = (char **)realloc(
            rrex3->matches, rrex3->match_capacity * sizeof(char *));
    }
    rrex3->matches[rrex3->match_count] = (char *)malloc(strlen(rrex3->str) + 1);
    strcpy(rrex3->matches[rrex3->match_count], rrex3->str);
    char *original_expr = rrex3->expr;
    char *original_str = rrex3->str;
    rrex3->expr++;
    rrex3->inside_parentheses = true;
    while (*rrex3->expr != ')' && !rrex3->exit) {
        rrex3_move(rrex3, false);
    }
    while (*rrex3->expr != ')') {
        rrex3->expr++;
    }
    rrex3->expr++;
    rrex3->inside_parentheses = false;

    char *previous_expr = rrex3->expr;
    rrex3->expr = original_expr;
    rrex3_set_previous(rrex3);
    rrex3->expr = previous_expr;
    if (rrex3->valid == false) {
        rrex3->str = original_str;
        free(rrex3->matches[rrex3->match_count]);
    } else {
        rrex3->matches[rrex3->match_count]
                      [strlen(rrex3->matches[rrex3->match_count]) -
                       strlen(rrex3->str)] = 0;
        rrex3->match_count++;
    }
#if RREX3_DEBUG == 1
    rprinty("\\l Parentheses end: %c:%c:%d\n", *rrex3->expr, *rrex3->str,
            rrex3->valid);
#endif
}

inline static void rrex3_reset(rrex3_t *rrex3) {
    rrex3_free_matches(rrex3);
    rrex3->valid = true;
    rrex3->pattern_error = false;
    rrex3->inside_brackets = false;
    rrex3->inside_parentheses = false;
    rrex3->exit = false;
    rrex3->previous.expr = NULL;
    rrex3->previous.str = NULL;
    rrex3->previous.bytecode = 0;
    rrex3->failed.expr = NULL;
    rrex3->failed.str = NULL;
    rrex3->failed.bytecode = 0;
    rrex3->match_from_start = false;
}

void rrex3_init(rrex3_t *rrex3) {
    for (__uint8_t i = 0; i < 254; i++) {
        rrex3->functions[i] = rrex3_cmp_literal;
        rrex3->slash_functions[i] = rrex3_cmp_literal;
    }
    rrex3->functions['?'] = rrex3_cmp_question_mark;
    rrex3->functions['^'] = rrex3_cmp_roof;
    rrex3->functions['$'] = rrex3_cmp_dollar;
    rrex3->functions['.'] = rrex3_cmp_dot;
    rrex3->functions['*'] = rrex3_cmp_asterisk;
    rrex3->functions['+'] = rrex3_cmp_plus;
    rrex3->functions['|'] = rrex3_cmp_pipe;
    rrex3->functions['\\'] = rrex3_cmp_slash;
    rrex3->functions['{'] = rrex3_cmp_range;
    rrex3->functions['['] = rrex3_cmp_brackets;
    rrex3->functions['('] = rrex3_cmp_parentheses;
    rrex3->slash_functions['w'] = rrex3_cmp_w;
    rrex3->slash_functions['W'] = rrex3_cmp_w_upper;
    rrex3->slash_functions['d'] = rrex3_cmp_d;
    rrex3->slash_functions['D'] = rrex3_cmp_d_upper;
    rrex3->slash_functions['s'] = rrex3_cmp_whitespace;
    rrex3->slash_functions['S'] = rrex3_cmp_whitespace_upper;
    rrex3->slash_functions['b'] = rrex3_cmp_word_start_or_end;
    rrex3->slash_functions['B'] = rrex3_cmp_word_not_start_or_end;
    rrex3->match_count = 0;
    rrex3->match_capacity = 0;
    rrex3->matches = NULL;
    rrex3->compiled = NULL;

    rrex3_reset(rrex3);
}

rrex3_t *rrex3_new() {
    rrex3_t *rrex3 = (rrex3_t *)malloc(sizeof(rrex3_t));

    rrex3_init(rrex3);

    return rrex3;
}

rrex3_t *rrex3_compile(rrex3_t *rrex, char *expr) {

    rrex3_t *rrex3 = rrex ? rrex : rrex3_new();

    char *compiled = (char *)malloc(strlen(expr) + 1);
    unsigned int count = 0;
    while (*expr) {
        if (*expr == '[' && *(expr + 2) == ']') {
            *compiled = *(expr + 1);
            expr++;
            expr++;
        } else if (*expr == '[' && *(expr + 1) == '0' && *(expr + 2) == '-' &&
                   *(expr + 3) == '9' && *(expr + 4) == ']') {
            *compiled = '\\';
            compiled++;
            *compiled = 'd';
            count++;
            expr++;
            expr++;
            expr++;
            expr++;
        } else {
            *compiled = *expr;
        }
        if (*compiled == '[') {
            // in_brackets = true;

        } else if (*compiled == ']') {
            // in_brackets = false;
        }
        expr++;
        compiled++;
        count++;
    }
    *compiled = 0;
    compiled -= count;
    rrex3->compiled = compiled;
    return rrex3;
}

inline static void rrex3_set_previous(rrex3_t *rrex3) {
    rrex3->previous.function = rrex3->function;
    rrex3->previous.expr = rrex3->expr;
    rrex3->previous.str = rrex3->str;
    rrex3->previous.bytecode = *rrex3->expr;
}

static bool rrex3_move(rrex3_t *rrex3, bool resume_on_fail) {
    char *original_expr = rrex3->expr;
    char *original_str = rrex3->str;
    rrex3->bytecode = *rrex3->expr;
    rrex3->function = rrex3->functions[(int)rrex3->bytecode];
    rrex3->function(rrex3);
    if (!*rrex3->expr && !*rrex3->str) {
        rrex3->exit = true;
        return rrex3->valid;
    } else if (!*rrex3->expr) {
        // rrex3->valid = true;
        return rrex3->valid;
    }
    if (rrex3->pattern_error) {
        rrex3->valid = false;
        return rrex3->valid;
    }
    if (resume_on_fail && !rrex3->valid && *rrex3->expr) {

        // rrex3_set_previous(rrex3);
        rrex3->failed.bytecode = rrex3->bytecode;
        rrex3->failed.function = rrex3->function;
        rrex3->failed.expr = original_expr;
        rrex3->failed.str = original_str;
        rrex3->bytecode = *rrex3->expr;
        rrex3->function = rrex3->functions[(int)rrex3->bytecode];
        rrex3->function(rrex3);

        if (!rrex3->valid && !rrex3->pattern_error) {

            if (*rrex3->str) {
                char *pipe_position = strstr(rrex3->expr, "|");
                if (pipe_position != NULL) {
                    rrex3->expr = pipe_position + 1;
                    rrex3->str = rrex3->_str;
                    rrex3->valid = true;
                    return true;
                }
            }
            if (rrex3->match_from_start) {
                rrex3->valid = false;
                return rrex3->valid;
            }
            if (!*rrex3->str++) {
                rrex3->valid = false;
                return rrex3->valid;
            }
            rrex3->expr = rrex3->_expr;
            if (*rrex3->str)
                rrex3->valid = true;
        }
    } else {
    }
    return rrex3->valid;
}

rrex3_t *rrex3(rrex3_t *rrex3, char *str, char *expr) {
#if RREX3_DEBUG == 1
    printf("Regex check: %s:%s:%d\n", expr, str, 1);
#endif
    bool self_initialized = false;
    if (rrex3 == NULL) {
        self_initialized = true;
        rrex3 = rrex3_new();
    } else {
        rrex3_reset(rrex3);
    }

    rrex3->_str = str;
    rrex3->_expr = rrex3->compiled ? rrex3->compiled : expr;
    rrex3->str = rrex3->_str;
    rrex3->expr = rrex3->_expr;
    while (*rrex3->expr && !rrex3->exit) {
        if (!rrex3_move(rrex3, true))
            return NULL;
    }
    rrex3->expr = rrex3->_expr;
    if (rrex3->valid) {

        return rrex3;
    } else {
        if (self_initialized) {
            rrex3_free(rrex3);
        }
        return NULL;
    }
}

void rrex3_test() {
    rrex3_t *rrex = rrex3_new();

    assert(rrex3(rrex, "\"stdio.h\"\"string.h\"\"sys/time.h\"",
                 "\"(.*)\"\"(.*)\"\"(.*)\""));

    assert(rrex3(rrex, "aaaaaaa", "a*a$"));

    // assert(rrex3("ababa", "a*b*a*b*a$"));
    assert(rrex3(rrex, "#include\"test.h\"a", "#include.*\".*\"a$"));
    assert(rrex3(rrex, "#include \"test.h\"a", "#include.*\".*\"a$"));
    assert(rrex3(rrex, "aaaaaad", "a*d$"));
    assert(rrex3(rrex, "abcdef", "abd?cdef"));
    assert(!rrex3(rrex, "abcdef", "abd?def"));
    assert(rrex3(rrex, "abcdef", "def"));
    assert(!rrex3(rrex, "abcdef", "^def"));
    assert(rrex3(rrex, "abcdef", "def$"));
    assert(!rrex3(rrex, "abcdef", "^abc$"));
    assert(rrex3(rrex, "aB!.#1", "......"));
    assert(!rrex3(rrex, "aB!.#\n", "      ......"));
    assert(!rrex3(rrex, "aaaaaad", "q+d$"));
    assert(rrex3(rrex, "aaaaaaa", "a+a$"));
    assert(rrex3(rrex, "aaaaaad", "q*d$"));
    assert(!rrex3(rrex, "aaaaaad", "^q*d$"));

    // Asterisk function
    assert(rrex3(rrex, "123321", "123*321"));
    assert(rrex3(rrex, "pony", "p*ony"));
    assert(rrex3(rrex, "pppony", "p*ony"));
    assert(rrex3(rrex, "ppony", "p*pony"));
    assert(rrex3(rrex, "pppony", "pp*pony"));
    assert(rrex3(rrex, "pppony", ".*pony"));
    assert(rrex3(rrex, "pony", ".*ony"));
    assert(rrex3(rrex, "pony", "po*ny"));
    // assert(rrex3(rrex,"ppppony", "p*pppony"));

    // Plus function
    assert(rrex3(rrex, "pony", "p+ony"));
    assert(!rrex3(rrex, "ony", "p+ony"));
    assert(rrex3(rrex, "ppony", "p+pony"));
    assert(rrex3(rrex, "pppony", "pp+pony"));
    assert(rrex3(rrex, "pppony", ".+pony"));
    assert(rrex3(rrex, "pony", ".+ony"));
    assert(rrex3(rrex, "pony", "po+ny"));

    // Slash functions
    assert(rrex3(rrex, "a", "\\w"));
    assert(!rrex3(rrex, "1", "\\w"));
    assert(rrex3(rrex, "1", "\\W"));
    assert(!rrex3(rrex, "a", "\\W"));
    assert(rrex3(rrex, "a", "\\S"));
    assert(!rrex3(rrex, " ", "\\s"));
    assert(!rrex3(rrex, "\t", "\\s"));
    assert(!rrex3(rrex, "\n", "\\s"));
    assert(rrex3(rrex, "1", "\\d"));
    assert(!rrex3(rrex, "a", "\\d"));
    assert(rrex3(rrex, "a", "\\D"));
    assert(!rrex3(rrex, "1", "\\D"));
    assert(rrex3(rrex, "abc", "\\b"));

    assert(rrex3(rrex, "abc", "\\babc"));
    assert(!rrex3(rrex, "abc", "a\\b"));
    assert(!rrex3(rrex, "abc", "ab\\b"));
    assert(!rrex3(rrex, "abc", "abc\\b"));
    assert(rrex3(rrex, "abc", "a\\Bbc"));
    assert(rrex3(rrex, "abc", "ab\\B"));
    assert(!rrex3(rrex, "1ab", "1\\Bab"));
    assert(rrex3(rrex, "abc", "a\\Bbc"));

    // Escaping of special chars
    assert(rrex3(rrex, "()+*.\\", "\\(\\)\\+\\*\\.\\\\"));

    // Pipe
    // assert(rrex3(rrex,"abc","abc|def"));
    assert(rrex3(rrex, "abc", "def|jkl|abc"));
    assert(rrex3(rrex, "abc", "abc|def"));

    assert(rrex3(rrex, "rhq", "def|rhq|rha"));
    assert(rrex3(rrex, "abc", "abc|def"));

    // Repeat
    assert(rrex3(rrex, "aaaaa", "a{4}"));

    assert(rrex3(rrex, "aaaa", "a{1,3}a"));

    // Range
    assert(rrex3(rrex, "abc", "[abc][abc][abc]$"));
    assert(rrex3(rrex, "def", "[^abc][^abc][^abc]$"));
    assert(rrex3(rrex, "defabc", "[^abc][^abc][^abc]abc"));
    assert(rrex3(rrex, "0-9", "0-9"));
    assert(rrex3(rrex, "55-9", "[^6-9]5-9$"));
    assert(rrex3(rrex, "a", "[a-z]$"));
    assert(rrex3(rrex, "A", "[A-Z]$"));
    assert(rrex3(rrex, "5", "[0-9]$"));
    assert(!rrex3(rrex, "a", "[^a-z]$"));
    assert(!rrex3(rrex, "A", "[^A-Z]$"));
    assert(!rrex3(rrex, "5", "[^0-9]$"));
    assert(rrex3(rrex, "123abc", "[0-9]*abc$"));
    assert(rrex3(rrex, "123123", "[0-9]*$"));

    // Parentheses

    assert(rrex3(rrex, "datadata", "(data)*"));

    assert(rrex3(rrex, "datadatapony", "(data)*pony$"));

    assert(!rrex3(rrex, "datadatapony", "(d*p*ata)*pond$"));
    assert(rrex3(rrex, "datadatadato", "(d*p*ata)*dato"));
    assert(rrex3(rrex, "datadatadato", "(d*p*ata)*dato$"));
    assert(!rrex3(rrex, "datadatadato", "(d*p*a*ta)*gato$"));

    // Matches
    assert(rrex3(rrex, "123", "(123)"));
    assert(!strcmp(rrex->matches[0], "123"));

    assert(rrex3(rrex, "123321a", "(123)([0-4][2]1)a$"));
    assert(!strcmp(rrex->matches[1], "321"));

    assert(rrex3(rrex, "123321a", "(123)([0-4][2]1)a$"));
    assert(!strcmp(rrex->matches[1], "321"));

    assert(rrex3(rrex, "aaaabc", "(.*)c"));

    assert(rrex3(rrex, "abcde", ".....$"));

    assert(rrex3(rrex, "abcdefghijklmnopqrstuvwxyz",
                 "..........................$"));
    // printf("(%d)\n", rrex->valid);

    assert(rrex3(rrex, "#include <stdio.h>", "#include.*<(.*)>"));
    assert(!strcmp(rrex->matches[0], "stdio.h"));
    assert(rrex3(rrex, "#include \"stdlib.h\"", "#include.\"(.*)\""));
    assert(!strcmp(rrex->matches[0], "stdlib.h"));
    assert(rrex3(rrex, "\"stdio.h\"\"string.h\"\"sys/time.h\"",
                 "\"(.*)\"\"(.*)\"\"(.*)\""));
    assert(!strcmp(rrex->matches[0], "stdio.h"));
    assert(!strcmp(rrex->matches[1], "string.h"));
    assert(!strcmp(rrex->matches[2], "sys/time.h"));

    assert(rrex3(rrex, "    #include <stdio.h>", "#include.+<(.+)>"));
    assert(!strcmp(rrex->matches[0], "stdio.h"));
    assert(rrex3(rrex, "    #include \"stdlib.h\"", "#include.+\"(.+)\""));
    assert(!strcmp(rrex->matches[0], "stdlib.h"));

    assert(rrex3(rrex, "    \"stdio.h\"\"string.h\"\"sys/time.h\"",
                 "\"(.+)\"\"(.+)\"\"(.+)\""));
    assert(!strcmp(rrex->matches[0], "stdio.h"));
    assert(!strcmp(rrex->matches[1], "string.h"));
    assert(!strcmp(rrex->matches[2], "sys/time.h"));

    assert(rrex3(rrex, "int abc ", "int (.*)[; ]?$"));
    assert(!strcmp(rrex->matches[0], "abc"));
    assert(rrex3(rrex, "int abc;", "int (.*)[; ]?$"));
    assert(!strcmp(rrex->matches[0], "abc"));
    assert(rrex3(rrex, "int abc", "int (.*)[; ]?$"));
    assert(!strcmp(rrex->matches[0], "abc"));

    rrex3_free(rrex);
}
#endif
#ifndef RARENA_H
#define RARENA_H

#include <stdlib.h>
#include <string.h>

typedef struct arena_t {
    unsigned char *memory;
    unsigned int pointer;
    unsigned int size;
} arena_t;

arena_t *arena_construct() {
    arena_t *arena = (arena_t *)rmalloc(sizeof(arena_t));
    arena->memory = NULL;
    arena->pointer = 0;
    arena->size = 0;
    return arena;
}

arena_t *arena_new(size_t size) {
    arena_t *arena = arena_construct();
    arena->memory = (unsigned char *)rmalloc(size);
    arena->size = size;
    return arena;
}

void *arena_alloc(arena_t *arena, size_t size) {
    if (arena->pointer + size > arena->size) {
        return NULL;
    }
    void *p = arena->memory + arena->pointer;
    arena->pointer += size;
    return p;
}

void arena_free(arena_t *arena) {
    // Just constructed and unused arena memory is NULL so no free needed
    if (arena->memory) {
        rfree(arena->memory);
    }
    rfree(arena);
}

void arena_reset(arena_t *arena) { arena->pointer = 0; }
#endif
#ifndef RCASE_H
#define RCASE_H
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define RCAMEL_CASE 1
#define RSNAKE_CASE 2
#define RINVALID_CASE 0
#define RCONST_TEST_T 4;

int rdetermine_case(const char *str) {
    int length = strlen(str);

    char p = 0;
    while (*str) {
        if (p == '_' && islower(*str))
            return RSNAKE_CASE;
        if (p != '_' && !isupper(p) && isupper(*str))
            return RCAMEL_CASE;
        p = *str;
        str++;
    }
    return RINVALID_CASE;

    if (length == 0) {
        return RINVALID_CASE;
    }
    if (strchr(str, '_')) {
        if (str[0] == '_' || str[length - 1] == '_' || strstr(str, "__")) {
            return RINVALID_CASE;
        }
        for (int i = 0; i < length; i++) {
            if (!islower(str[i]) && str[i] != '_') {
                return RINVALID_CASE;
            }
        }
        return RSNAKE_CASE;
    } else {

        if (!islower(str[0])) {
            return RINVALID_CASE;
        }
        for (int i = 1; i < length; i++) {
            if (str[i] == '_') {
                return RINVALID_CASE;
            }
            if (isupper(str[i]) && isupper(str[i - 1])) {
                return RINVALID_CASE;
            }
        }
        return RCAMEL_CASE;
    }
}

char *rsnake_to_camel(const char *snake_case) {
    int length = strlen(snake_case);
    char *camel_case = (char *)malloc(length + 1);
    int j = 0;
    int toUpper = 0;

    for (int i = 0; i < length; i++) {
        if (i > 0 && snake_case[i] == '_' && snake_case[i + 1] == 'T') {
            toUpper = 1;
            if (snake_case[i + 1] == 'T' &&
                (snake_case[i + 2] != '\n' || snake_case[i + 2] != '\0' ||
                 snake_case[i + 2] != ' ')) {

                toUpper = 0;
            }
        }
        if (snake_case[i] == '_' && snake_case[i + 1] != 't') {
            toUpper = 1;
            if (snake_case[i + 1] == 't' &&
                (snake_case[i + 2] != '\n' || snake_case[i + 2] != '\0' ||
                 snake_case[i + 2] != ' ')) {
                toUpper = 0;
            }
        } else if (snake_case[i] == '_' && snake_case[i + 1] == 't' &&
                   !isspace(snake_case[i + 2])) {
            toUpper = 1;
        } else if (snake_case[i] == '_' && snake_case[i + 1] == 'T' &&
                   !isspace(snake_case[i + 2])) {
            toUpper = 1;
            camel_case[j++] = '_';
            j++;
        } else {
            if (toUpper) {
                camel_case[j++] = toupper(snake_case[i]);
                toUpper = 0;
            } else {
                camel_case[j++] = snake_case[i];
            }
        }
    }

    camel_case[j] = '\0';
    return camel_case;
}
char *rcamel_to_snake(const char *camelCase) {
    int length = strlen(camelCase);
    char *snake_case = (char *)malloc(2 * length + 1);
    int j = 0;

    for (int i = 0; i < length; i++) {
        if (isupper(camelCase[i])) {
            if (i != 0) {
                snake_case[j++] = '_';
            }
            snake_case[j++] = tolower(camelCase[i]);
        } else {
            snake_case[j++] = camelCase[i];
        }
    }

    snake_case[j] = '\0';
    return snake_case;
}

char *rflip_case(char *content) {
    if (rdetermine_case(content) == RSNAKE_CASE) {
        return rcamel_to_snake(content);
    } else if (rdetermine_case(content) == RCAMEL_CASE) {
        return rsnake_to_camel(content);
    } else {
        rprintr("Could not determine case\n");
        return NULL;
    }
}

char *rflip_case_file(char *filepath) {
    size_t file_size = rfile_size(filepath);
    if (file_size == 0) {
        return NULL;
    }
    char *content = (char *)malloc(file_size);
    char *result = NULL;
    if (rfile_readb(filepath, content, file_size)) {
        result = rflip_case(content);
        if (result) {
            free(content);
            return result;
        } else {
            return content;
        }
    }
    return result;
}

int rcase_main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: rcase <file>\n");
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        char *result = rflip_case_file(argv[i]);
        if (result) {
            printf("%s\n", result);
            free(result);
        }
    }
    return 0;
}
#endif

#ifndef RTERM_H
#define RTERM_H
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
typedef struct winsize winsize_t;

typedef struct rshell_keypress_t {
    bool pressed;
    bool ctrl;
    bool shift;
    bool escape;
    char c;
    int ms;
    int fd;
} rshell_keypress_t;

typedef struct rterm_t {
    bool show_cursor;
    bool show_footer;
    int ms_tick;
    rshell_keypress_t key;
    void (*before_cursor_move)(struct rterm_t *);
    void (*after_cursor_move)(struct rterm_t *);
    void (*after_key_press)(struct rterm_t *);
    void (*before_key_press)(struct rterm_t *);
    void (*before_draw)(struct rterm_t *);
    void (*after_draw)(struct rterm_t *);
    void *session;
    unsigned long iterations;
    void (*tick)(struct rterm_t *);
    char *status_text;
    char *_status_text_previous;
    winsize_t size;
    struct {
        int x;
        int y;
        int pos;
        int available;
    } cursor;
} rterm_t;

typedef void (*rterm_event)(rterm_t *);

void rterm_init(rterm_t *rterm) {
    memset(rterm, 0, sizeof(rterm_t));
    rterm->show_cursor = true;
    rterm->cursor.x = 0;
    rterm->cursor.y = 0;
    rterm->ms_tick = 100;
    rterm->_status_text_previous = NULL;
}

void rterm_getwinsize(winsize_t *w) {
    // Get the terminal size
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, w) == -1) {
        perror("ioctl");
        exit(EXIT_FAILURE);
    }
}

void rrawfd(int fd) {
    struct termios orig_termios;
    tcgetattr(fd, &orig_termios); // Get current terminal attributes

    struct termios raw = orig_termios;
    raw.c_lflag &=
        ~(ICANON | ISIG | ECHO); // ECHO // Disable canonical mode and echoing
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 240; // Set timeout for read input

    tcsetattr(fd, TCSAFLUSH, &raw);
}

// Terminal setup functions
void enableRawMode(struct termios *orig_termios) {

    struct termios raw = *orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echoing
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 240; // Set timeout for read input

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disableRawMode(struct termios *orig_termios) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH,
              orig_termios); // Restore original terminal settings
}

void rterm_clear_screen() {
    printf("\x1b[2J"); // Clear the entire screen
    printf("\x1b[H");  // Move cursor to the home position (0,0)
}

void setBackgroundColor() {
    printf("\x1b[34m"); // Set background color to blue
}

void rterm_move_cursor(int x, int y) {

    printf("\x1b[%d;%dH", y + 1, x + 1); // Move cursor to (x, y)
}

void cursor_set(rterm_t *rt, int x, int y) {
    rt->cursor.x = x;
    rt->cursor.y = y;
    rt->cursor.pos = y * rt->size.ws_col + x;
    rterm_move_cursor(rt->cursor.x, rt->cursor.y);
}
void cursor_restore(rterm_t *rt) {
    rterm_move_cursor(rt->cursor.x, rt->cursor.y);
}

void rterm_print_status_bar(rterm_t *rt, char c, unsigned long i) {
    if (rt->_status_text_previous &&
        !strcmp(rt->_status_text_previous, rt->status_text)) {
        return;
    }
    if (rt->_status_text_previous) {
        free(rt->_status_text_previous);
    }
    rt->_status_text_previous = strdup(rt->status_text);
    winsize_t ws = rt->size;
    cursor_set(rt, rt->cursor.x, rt->cursor.y);
    rterm_move_cursor(0, ws.ws_row - 1);

    char output_str[1024];
    output_str[0] = 0;

    // strcat(output_str, "\x1b[48;5;240m");

    for (int i = 0; i < ws.ws_col; i++) {
        strcat(output_str, " ");
    }
    char content[500];
    content[0] = 0;
    if (!rt->status_text) {
        sprintf(content, "\rp:%d:%d | k:%c:%d | i:%ld ", rt->cursor.x + 1,
                rt->cursor.y + 1, c == 0 ? '0' : c, c, i);
    } else {
        sprintf(content, "\r%s", rt->status_text);
    }
    strcat(output_str, content);
    // strcat(output_str, "\x1b[0m");
    printf("%s", output_str);
    cursor_restore(rt);
}

void rterm_show_cursor() {
    printf("\x1b[?25h"); // Show the cursor
}

void rterm_hide_cursor() {
    printf("\x1b[?25l"); // Hide the cursor
}

rshell_keypress_t rshell_getkey(rterm_t *rt) {
    static rshell_keypress_t press;
    press.c = 0;
    press.ctrl = false;
    press.shift = false;
    press.escape = false;
    press.pressed = rfd_wait(0, rt->ms_tick);
    if (!press.pressed) {
        return press;
    }
    press.c = getchar();
    char ch = press.c;
    if (ch == '\x1b') {
        // Get detail
        ch = getchar();

        if (ch == '[') {
            // non char key:
            press.escape = true;

            ch = getchar(); // is a number. 1 if shift + arrow
            press.c = ch;
            if (ch >= '0' && ch <= '9')
                ch = getchar();
            press.c = ch;
            if (ch == ';') {
                ch = getchar();
                press.c = ch;
                if (ch == '5') {
                    press.ctrl = true;
                    press.c = getchar(); // De arrow
                }
            }
        } else if (ch == 27) {
            press.escape = true;
            press.c = ch;
        } else {
            press.c = ch;
        }
    }
    return press;
}

// Main function
void rterm_loop(rterm_t *rt) {
    struct termios orig_termios;
    tcgetattr(STDIN_FILENO, &orig_termios); // Get current terminal attributes
    enableRawMode(&orig_termios);

    int x = 0, y = 0; // Initial cursor position
    char ch = 0;

    ;
    while (1) {
        rterm_getwinsize(&rt->size);
        rt->cursor.available = rt->size.ws_col * rt->size.ws_row;
        if (rt->tick) {
            rt->tick(rt);
        }

        rterm_hide_cursor();
        setBackgroundColor();
        rterm_clear_screen();
        if (rt->before_draw) {
            rt->before_draw(rt);
        }
        rterm_print_status_bar(rt, ch, rt->iterations);
        if (rt->after_draw) {
            rt->after_draw(rt);
        }
        if (!rt->iterations || (x != rt->cursor.x || y != rt->cursor.y)) {
            if (rt->cursor.y == rt->size.ws_row) {
                rt->cursor.y--;
            }
            if (rt->cursor.y < 0) {
                rt->cursor.y = 0;
            }
            x = rt->cursor.x;
            y = rt->cursor.y;
            if (rt->before_cursor_move)
                rt->before_cursor_move(rt);
            cursor_set(rt, rt->cursor.x, rt->cursor.y);
            if (rt->after_cursor_move)
                rt->after_cursor_move(rt);
            // x = rt->cursor.x;
            // y = rt->cursor.y;
        }
        if (rt->show_cursor)
            rterm_show_cursor();

        fflush(stdout);

        rt->key = rshell_getkey(rt);
        if (rt->key.pressed && rt->before_key_press) {
            rt->before_key_press(rt);
        }
        rshell_keypress_t key = rt->key;
        ch = key.c;
        if (ch == 'q')
            break; // Press 'q' to quit
        if (key.c == -1) {
            nsleep(1000 * 1000);
        }
        // Escape
        if (key.escape) {
            switch (key.c) {
            case 65: // Move up
                if (rt->cursor.y > -1)
                    rt->cursor.y--;
                break;
            case 66: // Move down
                if (rt->cursor.y < rt->size.ws_row)
                    rt->cursor.y++;
                break;
            case 68: // Move left
                if (rt->cursor.x > 0)
                    rt->cursor.x--;
                if (key.ctrl)
                    rt->cursor.x -= 4;
                break;
            case 67: // Move right
                if (rt->cursor.x < rt->size.ws_col) {
                    rt->cursor.x++;
                }
                if (key.ctrl) {
                    rt->cursor.x += 4;
                }
                break;
            }
        }

        if (rt->key.pressed && rt->after_key_press) {
            rt->after_key_press(rt);
        }
        rt->iterations++;

        //  usleep (1000);
    }

    // Cleanup
    printf("\x1b[0m"); // Reset colors
    rterm_clear_screen();
    disableRawMode(&orig_termios);
}
#endif

#ifndef RTREE_H
#define RTREE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct rtree_t {
    struct rtree_t *next;
    struct rtree_t *children;
    char c;
    void *data;
} rtree_t;

rtree_t *rtree_new() {
    rtree_t *b = (rtree_t *)rmalloc(sizeof(rtree_t));
    b->next = NULL;
    b->children = NULL;
    b->c = 0;
    b->data = NULL;
    return b;
}

rtree_t *rtree_set(rtree_t *b, char *c, void *data) {
    while (b) {
        if (b->c == 0) {
            b->c = *c;
            c++;
            if (*c == 0) {
                b->data = data;
                // printf("SET1 %c\n", b->c);
                return b;
            }
        } else if (b->c == *c) {
            c++;
            if (*c == 0) {
                b->data = data;
                return b;
            }
            if (b->children) {
                b = b->children;
            } else {
                b->children = rtree_new();
                b = b->children;
            }
        } else if (b->next) {
            b = b->next;
        } else {
            b->next = rtree_new();
            b = b->next;
            b->c = *c;
            c++;
            if (*c == 0) {
                b->data = data;
                return b;
            } else {
                b->children = rtree_new();
                b = b->children;
            }
        }
    }
    return NULL;
}

rtree_t *rtree_find(rtree_t *b, char *c) {
    while (b) {
        if (b->c == *c) {
            c++;
            if (*c == 0) {
                return b;
            }
            b = b->children;
            continue;
        }
        b = b->next;
    }
    return NULL;
}

void rtree_free(rtree_t *b) {
    if (!b)
        return;
    rtree_free(b->children);
    rtree_free(b->next);
    rfree(b);
}

void *rtree_get(rtree_t *b, char *c) {
    rtree_t *t = rtree_find(b, c);
    if (t) {
        return t->data;
    }
    return NULL;
}
#endif
#ifndef RLEXER_H
#define RLEXER_H
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define RTOKEN_VALUE_SIZE 1024

typedef enum rtoken_type_t {
    RT_UNKNOWN = 0,
    RT_SYMBOL,
    RT_NUMBER,
    RT_STRING,
    RT_PUNCT,
    RT_OPERATOR,
    RT_EOF = 10,
    RT_BRACE_OPEN,
    RT_CURLY_BRACE_OPEN,
    RT_BRACKET_OPEN,
    RT_BRACE_CLOSE,
    RT_CURLY_BRACE_CLOSE,
    RT_BRACKET_CLOSE
} rtoken_type_t;

typedef struct rtoken_t {
    rtoken_type_t type;
    char value[RTOKEN_VALUE_SIZE];
    unsigned int line;
    unsigned int col;
} rtoken_t;

static char *_content;
static unsigned int _content_ptr;
static unsigned int _content_line;
static unsigned int _content_col;

static int isgroupingchar(char c) {
    return (c == '{' || c == '}' || c == '(' || c == ')' || c == '[' ||
            c == ']' || c == '"' || c == '\'');
}

static int isoperator(char c) {
    return (c == '+' || c == '-' || c == '/' || c == '*' || c == '=' ||
            c == '>' || c == '<' || c == '|' || c == '&');
}

static rtoken_t rtoken_new() {
    rtoken_t token;
    memset(&token, 0, sizeof(token));
    token.type = RT_UNKNOWN;
    return token;
}

rtoken_t rlex_number() {
    rtoken_t token = rtoken_new();
    token.col = _content_col;
    token.line = _content_line;
    bool first_char = true;
    int dot_count = 0;
    char c;
    while (isdigit(c = _content[_content_ptr]) ||
           (first_char && _content[_content_ptr] == '-') ||
           (dot_count == 0 && _content[_content_ptr] == '.')) {
        if (c == '.')
            dot_count++;
        first_char = false;
        char chars[] = {c, 0};
        strcat(token.value, chars);
        _content_ptr++;
        _content_col++;
    }
    token.type = RT_NUMBER;
    return token;
}

static rtoken_t rlex_symbol() {
    rtoken_t token = rtoken_new();

    token.col = _content_col;
    token.line = _content_line;
    char c;
    while (isalpha(_content[_content_ptr]) || _content[_content_ptr] == '_') {
        c = _content[_content_ptr];
        char chars[] = {c, 0};
        strcat(token.value, chars);
        _content_ptr++;
        _content_col++;
    }
    token.type = RT_SYMBOL;
    return token;
}

static rtoken_t rlex_operator() {

    rtoken_t token = rtoken_new();

    token.col = _content_col;
    token.line = _content_line;
    char c;
    bool is_first = true;
    while (isoperator(_content[_content_ptr])) {
        if (!is_first) {
            if (_content[_content_ptr - 1] == '=' &&
                _content[_content_ptr] == '-') {
                break;
            }
        }
        c = _content[_content_ptr];
        char chars[] = {c, 0};
        strcat(token.value, chars);
        _content_ptr++;
        _content_col++;
        is_first = false;
    }
    token.type = RT_OPERATOR;
    return token;
}

static rtoken_t rlex_punct() {

    rtoken_t token = rtoken_new();

    token.col = _content_col;
    token.line = _content_line;
    char c;
    bool is_first = true;
    while (ispunct(_content[_content_ptr])) {
        if (!is_first) {
            if (_content[_content_ptr] == '"') {
                break;
            }
            if (_content[_content_ptr] == '\'') {
                break;
            }
            if (isgroupingchar(_content[_content_ptr])) {
                break;
            }
            if (isoperator(_content[_content_ptr])) {
                break;
            }
        }
        c = _content[_content_ptr];
        char chars[] = {c, 0};
        strcat(token.value, chars);
        _content_ptr++;
        _content_col++;
        is_first = false;
    }
    token.type = RT_PUNCT;
    return token;
}

static rtoken_t rlex_string() {
    rtoken_t token = rtoken_new();
    char c;
    token.col = _content_col;
    token.line = _content_line;
    char str_chr = _content[_content_ptr];
    _content_ptr++;
    while (_content[_content_ptr] != str_chr) {
        c = _content[_content_ptr];
        if (c == '\\') {
            _content_ptr++;
            c = _content[_content_ptr];
            if (c == 'n') {
                c = '\n';
            } else if (c == 'r') {
                c = '\r';
            } else if (c == 't') {
                c = '\t';
            } else if (c == str_chr) {
                c = str_chr;
            }

            _content_col++;
        }
        char chars[] = {c, 0};
        strcat(token.value, chars);
        _content_ptr++;
        _content_col++;
    }
    _content_ptr++;
    token.type = RT_STRING;
    return token;
}

void rlex(char *content) {
    _content = content;
    _content_ptr = 0;
    _content_col = 1;
    _content_line = 1;
}

static void rlex_repeat_str(char *dest, char *src, unsigned int times) {
    for (size_t i = 0; i < times; i++) {
        strcat(dest, src);
    }
}

rtoken_t rtoken_create(rtoken_type_t type, char *value) {
    rtoken_t token = rtoken_new();
    token.type = type;
    token.col = _content_col;
    token.line = _content_line;
    strcpy(token.value, value);
    return token;
}

rtoken_t rlex_next() {
    while (true) {

        _content_col++;

        if (_content[_content_ptr] == 0) {
            return rtoken_create(RT_EOF, "eof");
        } else if (_content[_content_ptr] == '\n') {
            _content_line++;
            _content_col = 1;
            _content_ptr++;
        } else if (isspace(_content[_content_ptr])) {
            _content_ptr++;
        } else if (isdigit(_content[_content_ptr]) ||
                   (_content[_content_ptr] == '-' &&
                    isdigit(_content[_content_ptr + 1]))) {
            return rlex_number();
        } else if (isalpha(_content[_content_ptr]) ||
                   _content[_content_ptr] == '_') {
            return rlex_symbol();
        } else if (_content[_content_ptr] == '"' ||
                   _content[_content_ptr] == '\'') {
            return rlex_string();
        } else if (isoperator(_content[_content_ptr])) {
            return rlex_operator();
        } else if (ispunct(_content[_content_ptr])) {
            if (_content[_content_ptr] == '{') {

                _content_ptr++;
                return rtoken_create(RT_CURLY_BRACE_OPEN, "{");
            }
            if (_content[_content_ptr] == '}') {

                _content_ptr++;
                return rtoken_create(RT_CURLY_BRACE_CLOSE, "}");
            }
            if (_content[_content_ptr] == '(') {

                _content_ptr++;
                return rtoken_create(RT_BRACE_OPEN, "(");
            }
            if (_content[_content_ptr] == ')') {

                _content_ptr++;
                return rtoken_create(RT_BRACE_CLOSE, ")");
            }
            if (_content[_content_ptr] == '[') {

                _content_ptr++;
                return rtoken_create(RT_BRACKET_OPEN, "[");
            }
            if (_content[_content_ptr] == ']') {

                _content_ptr++;
                return rtoken_create(RT_BRACKET_CLOSE, "]");
            }
            return rlex_punct();
        }
    }
}

char *rlex_format(char *content) {
    rlex(content);
    char *result = (char *)malloc(strlen(content) + 4096);
    result[0] = 0;
    unsigned int tab_index = 0;
    char *tab_chars = "    ";
    unsigned int col = 0;
    rtoken_t token_previous;
    token_previous.value[0] = 0;
    token_previous.type = RT_UNKNOWN;
    while (true) {
        rtoken_t token = rlex_next();
        if (token.type == RT_EOF) {
            break;
        }

        // col = strlen(token.value);

        if (col == 0) {
            rlex_repeat_str(result, tab_chars, tab_index);
            // col = strlen(token.value);// strlen(tab_chars) * tab_index;
        }

        if (token.type == RT_STRING) {
            strcat(result, "\"");

            char string_with_slashes[strlen(token.value) * 2 + 1];
            rstraddslashes(token.value, string_with_slashes);
            strcat(result, string_with_slashes);

            strcat(result, "\"");
            // col+= strlen(token.value) + 2;
            // printf("\n");
            // printf("<<<%s>>>\n",token.value);

            memcpy(&token_previous, &token, sizeof(token));
            continue;
        }
        if (!(strcmp(token.value, "{"))) {
            if (col != 0) {
                strcat(result, "\n");
                rlex_repeat_str(result, "    ", tab_index);
            }
            strcat(result, token.value);

            tab_index++;

            strcat(result, "\n");

            col = 0;

            memcpy(&token_previous, &token, sizeof(token));
            continue;
        } else if (!(strcmp(token.value, "}"))) {
            unsigned int tab_indexed = 0;
            if (tab_index)
                tab_index--;
            strcat(result, "\n");

            rlex_repeat_str(result, tab_chars, tab_index);
            tab_indexed++;

            strcat(result, token.value);
            strcat(result, "\n");
            col = 0;

            memcpy(&token_previous, &token, sizeof(token));
            continue;
        }
        if ((token_previous.type == RT_SYMBOL && token.type == RT_NUMBER) ||
            (token_previous.type == RT_NUMBER && token.type == RT_SYMBOL) ||
            (token_previous.type == RT_PUNCT && token.type == RT_SYMBOL) ||
            (token_previous.type == RT_BRACE_CLOSE &&
             token.type == RT_SYMBOL) ||
            (token_previous.type == RT_SYMBOL && token.type == RT_SYMBOL)) {
            if (token_previous.value[0] != ',' &&
                token_previous.value[0] != '.') {
                if (token.type != RT_OPERATOR && token.value[0] != '.') {
                    strcat(result, "\n");
                    rlex_repeat_str(result, tab_chars, tab_index);
                }
            }
        }

        if (token.type == RT_OPERATOR) {
            strcat(result, " ");
        }
        if (token.type == RT_STRING) {
            strcat(result, "\"");
        }
        strcat(result, token.value);
        if (token.type == RT_STRING) {
            strcat(result, "\"");
        }

        if (token.type == RT_OPERATOR) {
            strcat(result, " ");
        }
        if (!strcmp(token.value, ",")) {
            strcat(result, " ");
        }
        col += strlen(token.value);
        memcpy(&token_previous, &token, sizeof(token));
    }
    return result;
}
#endif

#ifndef RLIB_MAIN
#define RLIB_MAIN
#ifndef RMERGE_H
#define RMERGE_H
// #include "../mrex/rmatch.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool has_error = false;

char *extract_script_src_include(char *line, char *include_path) {
    include_path[0] = 0;
    rrex3_t *rrex;
    rrex = rrex3(NULL, line, "<script.*src=\"(.*)\".*<.*script.*>");
    if (rrex) {
        strcpy(include_path, rrex->matches[0]);
        rrex3_free(rrex);
        return include_path;
    }
    return NULL;
}

char *extract_c_local_include(char *line, char *include_path) {
    //
    /*
        char res;
        res= rmatch_extract(line, "#include.*"\".*\"");


        printf("%MATCH:%s\n", res);
    */

    include_path[0] = 0;
    rrex3_t *rrex;
    rrex = rrex3(NULL, line, "[^\\\\*]^#include .*\"(.*)\"");
    if (rrex) {
        strcpy(include_path, rrex->matches[0]);
        rrex3_free(rrex);
        return include_path;
    }
    return NULL;
}

char *readline(FILE *f) {
    static char data[4096];
    data[0] = 0;
    int index = 0;
    char c;
    while ((c = fgetc(f)) != EOF) {
        if (c != '\0') {
            data[index] = c;
            index++;
            if (c == '\n')
                break;
        }
    }
    data[index] = 0;
    if (data[0] == 0)
        return NULL;
    return data;
}
void writestring(FILE *f, char *line) {
    char c;
    while ((c = *line) != '\0') {
        fputc(c, f);
        line++;
    }
}
char files_history[8096];
char files_duplicate[8096];
bool is_merging = false;

void merge_file(char *source, FILE *d) {
    if (is_merging == false) {
        is_merging = true;
        files_history[0] = 0;
        files_duplicate[0] = 0;
    }
    if (strstr(files_history, source)) {
        if (strstr(files_duplicate, source)) {
            rprintmf(stderr,
                     "\\l Already included: %s. Already on duplicate list.\n",
                     source);
        } else {
            rprintcf(stderr,
                     "\\l Already included: %s. Adding to duplicate list.\n",
                     source);
            strcat(files_duplicate, source);
            strcat(files_duplicate, "\n");
        }
        return;
    } else {
        rprintgf(stderr, "\\l Merging: %s.\n", source);
        strcat(files_history, source);
        strcat(files_history, "\n");
    }
    FILE *fd = fopen(source, "rb");
    if (!fd) {
        rprintrf(stderr, "\\l File does not exist: %s\n", source);
        has_error = true;
        return;
    }

    char *line;
    char include_path[4096];
    while ((line = readline(fd))) {

        include_path[0] = 0;
        if (!*line)
            break;

        //
        char *inc = extract_c_local_include(line, include_path);
        if (!inc)
            inc = extract_script_src_include(line, include_path);

        /*
         if (!strncmp(line, "#include ", 9)) {
             int index = 0;
             while (line[index] != '"' && line[index] != 0) {
                 index++;
             }
             if (line[index] == '"') {
                 int pindex = 0;
                 index++;
                 while (line[index] != '"') {
                     include_path[pindex] = line[index];
                     pindex++;
                     index++;
                 }
                 if (line[index] != '"') {
                     include_path[0] = 0;
                 } else {
                     include_path[pindex] = '\0';
                 }
             }
         }*/
        if (inc) {
            merge_file(inc, d);
        } else {
            writestring(d, line);
        }
    }
    fclose(fd);
    writestring(d, "\n");
}

int rmerge_main(int argc, char *argv[]) {
    char *file_input = NULL;
    if (argc != 2) {
        printf("Usage: <input-file>\n");
    } else {
        file_input = argv[1];
        // file_output = argv[2];
    }
    FILE *f = tmpfile();
    printf("// RETOOR - %s\n", __DATE__);
    merge_file(file_input, f);
    rewind(f);
    char *data;
    int line_number = 0;
    while ((data = readline(f))) {
        if (line_number) {
            printf("/*%.5d*/    ", line_number);
            line_number++;
        }
        printf("%s", data);
    }
    printf("\n");
    if (has_error) {
        rprintrf(stderr,
                 "\\l Warning: there are errors while merging this file.\n");
    } else {
        rprintgf(stderr, "\\l Merge succesful without error(s).\n");
    }
    return 0;
}
#endif

void forward_argument(int *argcc, char *argv[]) {
    int argc = *argcc;
    for (int i = 0; i < argc; i++) {
        argv[i] = argv[i + 1];
    }
    argc--;
    *argcc = argc;
}

int rlib_main(int argc, char *argv[]) {

    if (argc == 1) {
        printf("rlib\n\n");
        printf("options:\n");
        printf(" httpd - a http file server. Accepts port as argument.\n");
        printf(
            " rmerge - a merge tool. Converts c source files to one file \n"
            "          with local includes by giving main file as argument.\n");
        printf(" rcov - coverage tool theat cleans up after himself. Based on "
               "lcov.\n");
        printf(" rcase - tool to swap input file automatically between"
               "         camel case and snake case.\n");
        return 0;
    }

    forward_argument(&argc, argv);

    if (!strcmp(argv[0], "httpd")) {

        return rhttp_main(argc, argv);
    }
    if (!strcmp(argv[0], "rmerge")) {
        return rmerge_main(argc, argv);
    }
    if (!strcmp(argv[0], "rcov")) {
        return rcov_main(argc, argv);
    }
    if (!strcmp(argv[0], "rcase")) {
        return rcase_main(argc, argv);
    }

    return 0;
}

#endif

// END OF RLIB
#endif
