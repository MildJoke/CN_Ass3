#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <ctype.h>
#include <stdint.h>
#include <time.h>

#define MAX_FLIGHT_NUM             15
#define SERVER_LISTEN_PORT         "8080"
#define MAX_BACKLOG                10
#define MAX_FDS                    5

unsigned int minimum(unsigned int x, unsigned int y) {
    return x < y ? x : y;
}

unsigned int factorial(unsigned int n) {
    unsigned int result = 1;
    for (unsigned int j = 2; j <= minimum(n, 20); j++) {
        result *= j;
    }
    return result;
}

void handle_error(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main() {
    struct addrinfo hints, *servinfo;
    int server_socket, yes = 1;
    struct pollfd fds[MAX_FDS];
    int nfds = 1, new_fd;

    openlog("flight-time-server", LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER);
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, SERVER_LISTEN_PORT, &hints, &servinfo) != 0) {
        handle_error("getaddrinfo");
    }

    for (struct addrinfo *p = servinfo; p != NULL; p = p->ai_next) {
        server_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (server_socket == -1) continue;

        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            handle_error("setsockopt");
        }

        if (bind(server_socket, p->ai_addr, p->ai_addrlen) == 0) break;

        close(server_socket);
    }

    if (servinfo == NULL) {
        handle_error("server: failed to bind");
    }

    freeaddrinfo(servinfo);

    if (listen(server_socket, MAX_BACKLOG) == -1) {
        handle_error("listen");
    }

    fds[0].fd = server_socket;
    fds[0].events = POLLIN;

    while (1) {
        if (poll(fds, nfds, -1) == -1) {
            handle_error("poll");
        }

        for (int i = 0; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == server_socket) {
                    struct sockaddr_storage client_addr;
                    socklen_t addr_size = sizeof client_addr;
                    new_fd = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
                    if (new_fd == -1) {
                        handle_error("accept");
                    } else {
                        if (nfds == MAX_FDS) {
                            handle_error("too many clients");
                        }
                        fds[nfds].fd = new_fd;
                        fds[nfds].events = POLLIN;
                        nfds++;
                    }
                } else {
                    char buffer[1024];
                    ssize_t bytes_received = recv(fds[i].fd, buffer, sizeof buffer, 0);
                    if (bytes_received <= 0) {
                        if (bytes_received == 0) {
                            printf("server: socket %d hung up\n", fds[i].fd);
                        } else {
                            handle_error("recv");
                        }
                        close(fds[i].fd);
                        fds[i] = fds[nfds - 1];
                        nfds--;
                    } else {
                        buffer[bytes_received] = '\0';
                        unsigned int num = atoi(buffer);
                        unsigned int result = factorial(num);
                        char result_str[20];
                        snprintf(result_str, sizeof result_str, "%u", result);
                        if (send(fds[i].fd, result_str, strlen(result_str), 0) == -1) {
                            handle_error("send");
                        }
                    }
                }
            }
        }
    }

    closelog();
    return 0;
}

void handle_error(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}
