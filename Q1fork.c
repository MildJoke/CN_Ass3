#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SERVER_PORT 8080

unsigned int min_value(unsigned int x, unsigned int y){
    return x < y ? x : y;
}

unsigned int factorial(unsigned int num){
    unsigned int product = 1;
    for(unsigned int k = 2; k <= min_value(num, 20); k++){
        product *= k;
    }
    return product;
}

int main() {
    int server_fd, bind_status;
    struct sockaddr_in serverAddress, clientAddress;
    int connection_fd;
    socklen_t client_addr_len = sizeof(clientAddress);
    pid_t child_process_id;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Failed to create server socket");
        exit(EXIT_FAILURE);
    }
    printf("Server Socket is ready.\n");
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);
    serverAddress.sin_addr.s_addr = inet_addr("10.0.2.15");
    bind_status = bind(server_fd, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    if (bind_status < 0) {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 10) == 0) {
        printf("Server is listening for connections...\n");
    } else {
        perror("Failed to listen");
        exit(EXIT_FAILURE);
    }
    int client_count = 0;
    while (1) {
        connection_fd = accept(server_fd, (struct sockaddr*)&clientAddress, &client_addr_len);
        if (connection_fd < 0) {
            perror("Failed to accept client connection");
            continue;
        }
        printf("Client connected from %s:%d\n", inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));
        printf("Total connected clients: %d\n", ++client_count);
        if ((child_process_id = fork()) == 0) {
            close(server_fd);
            char client_msg[1024];
            ssize_t msg_size;
            const char *welcome_msg = "Welcome, client!";
            send(connection_fd, welcome_msg, strlen(welcome_msg), 0);
            while (1) {
                msg_size = recv(connection_fd, client_msg, sizeof(client_msg) - 1, 0);
                if (msg_size <= 0) {
                    break;
                }
                client_msg[msg_size] = '\0';
                unsigned int num_to_factor = (unsigned int)atoi(client_msg);
                unsigned int factorial_result = factorial(num_to_factor);
                char result_str[1024];
                snprintf(result_str, sizeof(result_str), "%u\n", factorial_result);
                send(connection_fd, result_str, strlen(result_str), 0);
            }
            close(connection_fd);
            exit(EXIT_SUCCESS);
        }
        close(connection_fd);
    }
    close(server_fd);
    return 0;
}
