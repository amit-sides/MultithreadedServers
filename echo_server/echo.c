
/**
 ** Written by Amit Sides
 **/

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define SERVER_INTERFACE    ("0.0.0.0")
#define SERVER_PORT         (12345)

#define MAX_MESSAGE_SIZE    (256)

int setup_server();

void handle_client(int client_fd, struct sockaddr_in client_address)
{
    int bytes_recv = 0;
    int bytes_sent = 0;
    char message[MAX_MESSAGE_SIZE] = {0};

    // Print client information
    char *client_ip = inet_ntoa(client_address.sin_addr);
    int client_port = ntohs(client_address.sin_port);
    printf("A client connected from: %s:%d!\n", client_ip, client_port);

    while(true)
    {
        // Receive client message
        bytes_recv = recv(client_fd, message, sizeof(message), 0);
        if (-1 == bytes_recv)
        {
            perror("recv failed");
            goto lbl_cleanup;
        }

        if (0 == bytes_recv)
        {
            // Connection probably closed...
            printf("client %s:%d disconnected.\n", client_ip, client_port);
            goto lbl_cleanup;
        }

        // A message was received, print it.
        message[bytes_recv-1] = '\0'; // replacing new-line with null-terminator
        printf("Echoing '%s' to %s:%d...\n", message, client_ip, client_port);
        message[bytes_recv-1] = '\n'; // replacing null-terminator with new-line

        // Echoing the message...
        bytes_sent = send(client_fd, message, bytes_recv, 0);
        if (bytes_sent != bytes_recv)
        {
            perror("Failed to echo message");
            goto lbl_cleanup;
        }
    }

lbl_cleanup:
    close(client_fd);
}

int main()
{
    struct sockaddr_in client_address = {0};
    int server_fd = 0, client_fd = 0;
    int client_address_size = sizeof(client_address);

    // Setup the server (socket, bind, listen)
    server_fd = setup_server();

    // Keep the server running to accept new clients
    while (true)
    {
        // Accept a client
        client_fd = accept4(server_fd, (struct sockaddr *)&client_address,
                            &client_address_size, SOCK_CLOEXEC);
        if (0 < client_fd)
        {
            handle_client(client_fd, client_address);
        }
    }

    close(server_fd);
    return 0;
}


int setup_server()
{
    struct sockaddr_in server_address = {0};
    int server_fd = 0;

    // Creates the server socket
    server_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (-1 == server_fd) {
        perror("Failed to create server socket");
        exit(errno);
    }

    // Sets socket option SO_REUSEADDR, other processes will be able to listen on this port
    // (enables quick restart of the server)
    int enabled = 1;
    if (0 != setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)))
    {
        perror("Failed to set SO_REUSEADDR");
        exit(errno);
    }

    // Construct the server's data struct
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(SERVER_INTERFACE);
    server_address.sin_port = htons(SERVER_PORT);

    // Bind server socket to the network interface
    if (0 != bind(server_fd, (struct sockaddr *)&server_address, sizeof(server_address))) {
        perror("Failed to bind");
        exit(errno);
    }

    // Listen for incoming clients
    if (0 != listen(server_fd, 0)) {
        perror("Failed to listen");
        exit(errno);
    }

    printf("Listening for incoming connections on %s:%d...\n", SERVER_INTERFACE, SERVER_PORT);
    return server_fd;
}
