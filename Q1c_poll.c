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
#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>
#include <time.h>

#define MAX_FLIGHT_NUM_LEN        15
#define PORT_NUMBER               "4444"
#define QUEUE_SIZE                10
#define POLL_ARRAY_SIZE           5

void handle_error (char *error_msg);

unsigned int min_value(unsigned int x, unsigned int y){
    return x < y ? x : y;
}

unsigned int compute_factorial(unsigned int val){
    unsigned int result = 1;
    for(unsigned int k = 2; k <= min_value(val, 20); k++){
        result *= k;
    }
    return result;
}


struct comm_packet packet_recv, packet_send;

void handle_error(char *error_msg)
{
    perror(error_msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    struct addrinfo addr_setup, *addr_result;
    memset(&addr_setup, 0, sizeof(struct addrinfo));
    addr_setup.ai_family = AF_UNSPEC;
    addr_setup.ai_socktype = SOCK_STREAM;
    addr_setup.ai_flags = AI_PASSIVE;

    int addr_info_status, listen_socket, reuse_addr = 1;
    struct addrinfo *addr_iter;
    if ((addr_info_status = getaddrinfo(NULL, PORT_NUMBER, &addr_setup, &addr_result)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addr_info_status));
        exit(EXIT_FAILURE);
    }

    for (addr_iter = addr_result; addr_iter != NULL; addr_iter = addr_iter->ai_next) {
        listen_socket = socket(addr_iter->ai_family, addr_iter->ai_socktype, addr_iter->ai_protocol);
        if (listen_socket == -1) continue;

        if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(int)) == -1)
            handle_error("setsockopt");

        if (bind(listen_socket, addr_iter->ai_addr, addr_iter->ai_addrlen) == 0) break;

        close(listen_socket);
    }

    if (addr_iter == NULL) {
        fprintf(stderr, "Failed to bind\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(addr_result);

    if (listen(listen_socket, QUEUE_SIZE) == -1)
        handle_error("listen");

    struct pollfd *fds_list;
    int max_fd_count = 0, current_fd_count = 0;

    if ((fds_list = malloc(POLL_ARRAY_SIZE * sizeof(struct pollfd))) == NULL)
        handle_error("malloc");
    max_fd_count = POLL_ARRAY_SIZE;

    fds_list->fd = listen_socket;
    fds_list->events = POLLIN;
    fds_list->revents = 0;
    current_fd_count = 1;

    struct sockaddr_storage client_addr;
    char ip_str[INET6_ADDRSTRLEN];
    struct sockaddr_in *ipv4_ptr;
    struct sockaddr_in6 *ipv6_ptr;

    while (1) {
        if (poll(fds_list, current_fd_count, -1) == -1)
            handle_error("poll");

        for (int current_fd = 0; current_fd < current_fd_count; current_fd++) {
            if (fds_list[current_fd].revents & POLLIN) {
                if (fds_list[current_fd].fd == listen_socket) {
                    int new_socket = accept(listen_socket, NULL, NULL);
                    if (new_socket == -1)
                        handle_error("accept");

                    if (current_fd_count == max_fd_count) {
                        max_fd_count += POLL_ARRAY_SIZE;
                        fds_list = realloc(fds_list, max_fd_count * sizeof(struct pollfd));
                        if (fds_list == NULL) handle_error("realloc");
                    }

                    fds_list[current_fd_count].fd = new_socket;
                    fds_list[current_fd_count].events = POLLIN;
                    current_fd_count++;
                } else {
                    char buffer[1024];
                    ssize_t recv_bytes = recv(fds_list[current_fd].fd, buffer, sizeof(buffer), 0);
                    if (recv_bytes == -1) {
                        handle_error("recv");
                    } else if (recv_bytes == 0) {
                        close(fds_list[current_fd].fd);
                        fds_list[current_fd].fd = -1;
                    } else {
                        buffer[recv_bytes] = '\0';
                        unsigned int input_num = atoi(buffer);
                        unsigned int result = compute_factorial(input_num);
                        char response[1024];
                        sprintf(response, "%u", result);
                        send(fds_list[current_fd].fd, response, strlen(response), 0);
                    }
                }
            }
        }
    }
    close(listen_socket);
    return 0;
}
