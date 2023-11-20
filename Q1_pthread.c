#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>

#define SERVICE_PORT 4444

unsigned int minimum(unsigned int x, unsigned int y){
    return x < y ? x : y;
}

unsigned int calculate_factorial(unsigned int val){
    unsigned int product = 1;
    for(unsigned int k = 2; k <= minimum(val, 20); k++){
        product *= k;
    }
    return product;
}

void *handle_client(void *socket_desc) {
    int sock = *((int *)socket_desc);
    free(socket_desc);
    char msg[1024];
    int n_read;

    send(sock, "hello client", strlen("hello client"), 0);

    while (1) {
        n_read = recv(sock, msg, sizeof(msg), 0);
        if (n_read <= 0) {
            break;
        } else {
            msg[n_read] = '\0';
            unsigned int num = atoi(msg);
            unsigned int factorial = calculate_factorial(num);
            char num_str[1024];
            snprintf(num_str, sizeof(num_str), "%u", factorial);
            send(sock, num_str, strlen(num_str), 0);
        }
    }
    close(sock);
    return NULL;
}

int main() {
    int server_fd, bind_result;
    struct sockaddr_in server_address, client_address;
    int new_socket;
    socklen_t address_length;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Error in connection");
        exit(EXIT_FAILURE);
    }
    printf("Server Socket is created\n");
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVICE_PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;
    bind_result = bind(server_fd, (struct sockaddr *)&server_address, sizeof(server_address));
    if (bind_result < 0) {
        perror("Error in binding");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 10) == 0) {
        printf("Listening\n\n");
    }
    int client_counter = 0;
    while (1) {
        address_length = sizeof(client_address);
        new_socket = accept(server_fd, (struct sockaddr *)&client_address, &address_length);
        if (new_socket < 0) {
            exit(EXIT_FAILURE);
        }
        printf("Connection accepted from %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
        printf("Clients connected: %d\n\n", ++client_counter);
        int *new_sock = malloc(sizeof(int));
        *new_sock = new_socket;
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *)new_sock);
        pthread_detach(thread_id);
    }
    close(server_fd);
    return 0;
}
