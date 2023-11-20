#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>
#include <time.h>

#define MAX_FLIGHT_NUM            15
#define SERVER_PORT_STR           "8080"
#define LISTEN_BACKLOG            10
#define MAX_EPOLL_EVENTS          10

unsigned int minimum(unsigned int x, unsigned int y){
    return x < y ? x : y;
}

unsigned int factorial(unsigned int n){
    unsigned int result = 1;
    for(unsigned int i = 2; i <= minimum(n, 20); i++){
        result *= i;
    }
    return result;
}

struct recv_buffer, send_buffer;

void handleError (char *errorMessage);

int main (int argc, char **argv)
{
    struct addrinfo hints, *server_info;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status, server_socket, yes = 1;
    struct addrinfo *p;
    if ((status = getaddrinfo(NULL, SERVER_PORT_STR, &hints, &server_info)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    for (p = server_info; p != NULL; p = p->ai_next) {
        server_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (server_socket == -1) continue;

        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
            handleError("setsockopt");

        if (bind(server_socket, p->ai_addr, p->ai_addrlen) == 0) break;

        close(server_socket);
    }

    if (p == NULL) {
        fprintf(stderr, "Failed to bind\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(server_info);

    if (listen(server_socket, LISTEN_BACKLOG) == -1)
        handleError("listen");

    int epoll_fd;
    if ((epoll_fd = epoll_create1(0)) == -1)
        handleError("epoll_create1");

    struct epoll_event event_setup, events[MAX_EPOLL_EVENTS];

    event_setup.events = EPOLLIN;
    event_setup.data.fd = server_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event_setup) == -1)
        handleError("epoll_ctl");

    int num_fds;
    struct sockaddr_storage client_addr;
    char client_ip[INET6_ADDRSTRLEN];

    while (1) {
        num_fds = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, -1);
        if (num_fds == -1)
            handleError("epoll_wait");

        for (int i = 0; i < num_fds; i++) {
            if (events[i].events & EPOLLIN) {
                if (events[i].data.fd == server_socket) {
                    int client_fd = accept(server_socket, (struct sockaddr *)&client_addr, &(socklen_t){sizeof(client_addr)});
                    if (client_fd == -1)
                        handleError("accept");

                    event_setup.events = EPOLLIN;
                    event_setup.data.fd = client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event_setup) == -1)
                        handleError("epoll_ctl");

                    inet_ntop(client_addr.ss_family, &(((struct sockaddr_in *)&client_addr)->sin_addr), client_ip, sizeof(client_ip));
                }
                else {
                    char buffer[1024];
                    ssize_t bytes_received = recv(events[i].data.fd, buffer, sizeof(buffer), 0);
                    if (bytes_received == -1)
                        handleError("recv");
                    else if (bytes_received == 0) {
                        close(events[i].data.fd);
                    }
                    else {
                        buffer[bytes_received] = '\0';
                        unsigned int input_number = atoi(buffer);
                        unsigned int answer = factorial(input_number);
                        char response[1024];
                        snprintf(response, sizeof(response), "%u", answer);
                        if (send(events[i].data.fd, response, strlen(response), 0) == -1)
                            handleError("send");
                    }
                }
            }
        }
    }
    close(server_socket);
    return 0;
}

void handleError(char *errorMessage)
{
    perror(errorMessage);
    exit(EXIT_FAILURE);
}
