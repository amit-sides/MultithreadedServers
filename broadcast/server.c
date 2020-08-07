
/**
 ** Written by Amit Sides
 **/

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>

#include "common.h"
#include "colors.h"

#define NO_SOCKET           (-1)
#define SERVER_INTERFACE    ("0.0.0.0")
#define MAXIMUM_CLIENTS     (sizeof(colors) / sizeof(*colors)) // = 6


#define WELCOME_BANNER      ("Hello! Please enter your name: ")

int setup_server(int port);
int setup_broadcast();

typedef struct client_s {
    int client_fd;
    char name[MAX_NAME_SIZE + 1];
} client_t;

void send_to_all_clients(client_t *clients, char *message, size_t length)
{
    int bytes_sent = 0;
    int bytes_to_send = 0;
    char send_message[MAX_NAME_SIZE + MAX_MESSAGE_SIZE + 10] = {0}; // +10 for metadata

    printf("%s", message);
    for(int i=0; i < MAXIMUM_CLIENTS; i++)
    {
        if (NO_SOCKET == clients[i].client_fd)
        {
            continue;
        }

        errno = 0;
        bytes_sent = send(clients[i].client_fd, message, length, 0);
        if (ECONNRESET == errno || ETIMEDOUT == errno)
        {
            // Client closed the connection

            // Constructing a message to notify clients a client has disconnected.
            bytes_to_send = snprintf(send_message, sizeof(send_message), "%sServer:\tClient %s%s%s%s %swas disconnected.%s\n",
                                     BOLD_WHITE, RESET, colors[i], clients[i].name, RESET, BOLD_WHITE, RESET);

            close(clients[i].client_fd);
            clients[i].client_fd = NO_SOCKET;
            memset(clients[i].name, 0, sizeof(clients[i].name));
            send_to_all_clients(clients, send_message, bytes_to_send);
        }
        else if (bytes_sent != length)
        {
            perror("Failed to send message");
            exit(-1);
        }
    }
}

void get_client_name(client_t *clients, int client_index)
{
    int bytes_recv = 0;
    char send_message[MAX_NAME_SIZE + MAX_MESSAGE_SIZE + 10] = {0}; // +10 for metadata

    // Receive client name
    bytes_recv = recv(clients[client_index].client_fd, clients[client_index].name, MAX_NAME_SIZE, 0);
    if (-1 == bytes_recv && ECONNRESET != errno && ETIMEDOUT != errno)
    {
        perror("recv failed");
        exit(-1);
    }

    if (0 == bytes_recv || (-1 == bytes_recv && (ECONNRESET == errno || ETIMEDOUT == errno)))
    {
        // Client disconnected
        close(clients[client_index].client_fd);
        clients[client_index].client_fd = NO_SOCKET;
        memset(clients[client_index].name, 0, sizeof(clients[client_index].name));
        return;
    }

    // null-terminating the name
    clients[client_index].name[bytes_recv-1] = '\0';

    // Notifying the chat room a new client has connected
    bytes_recv = snprintf(send_message, sizeof(send_message), "%sServer:\tClient %s%s%s%s %shas connected.%s\n",
                          BOLD_WHITE, RESET, colors[client_index], clients[client_index].name, RESET, BOLD_WHITE, RESET);
    send_to_all_clients(clients, send_message, bytes_recv);
}

void handle_client(client_t *clients, int client_index)
{
    int bytes_recv = 0;
    int bytes_to_send = 0;
    char recv_message[MAX_MESSAGE_SIZE] = {0};
    char send_message[MAX_NAME_SIZE + MAX_MESSAGE_SIZE + 10] = {0}; // +10 for metadata

    // Checks if the client has entered a name already
    if ('\0' == clients[client_index].name[0])
    {
        // A client has no name
        get_client_name(clients, client_index);
        return;
    }

    // Receive client message
    bytes_recv = recv(clients[client_index].client_fd, recv_message, sizeof(recv_message), 0);
    if (-1 == bytes_recv && ECONNRESET != errno && ETIMEDOUT != errno)
    {
        perror("recv failed");
        exit(-1);
    }

    // We got an event on the fd, but no data was sent
    if (0 == bytes_recv || (-1 == bytes_recv && (ECONNRESET == errno || ETIMEDOUT == errno)))
    {
        // Connection probably closed...
        // Constructing a message to notify clients a client has disconnected.
        bytes_to_send = snprintf(send_message, sizeof(send_message), "%sServer:\tClient %s%s%s%s %swas disconnected.%s\n",
                              BOLD_WHITE, RESET, colors[client_index], clients[client_index].name, RESET, BOLD_WHITE, RESET);
        // Resetting client struct
        close(clients[client_index].client_fd);
        clients[client_index].client_fd = NO_SOCKET;
        memset(clients[client_index].name, 0, sizeof(clients[client_index].name));
    }
    else
    {
        // A message was received. Construct a message to deliver to the other clients.
        recv_message[bytes_recv-1] = '\0'; // replacing new-line with null-terminator
        // Constructing the message to send to the users
        bytes_to_send = snprintf(send_message, sizeof(send_message), "%s%s:\t%s%s\n",
                              colors[client_index], clients[client_index].name, recv_message, RESET);

    }

    // Checks the call to snprintf was successful
    if (0 > bytes_to_send)
    {
        perror("snprintf failed");
        exit(-1);
    }

    // Sending the message to all clients...
    send_to_all_clients(clients, send_message, bytes_to_send);
}

int handle_poll(int server_fd, int broadcast_fd, client_t *clients, struct pollfd *poll_fds)
{
    // Initializing the poll_fds array
    poll_fds[0].fd = server_fd;
    poll_fds[0].events = POLLIN | POLLHUP;
    poll_fds[1].fd = broadcast_fd;
    poll_fds[1].events = POLLIN | POLLHUP;
    for(int i=2; i < MAXIMUM_CLIENTS + 2; i++)
    {
        poll_fds[i].fd = clients[i-2].client_fd;
        poll_fds[i].events = POLLIN | POLLHUP;
    }

    // Calling poll with array
    return poll(poll_fds, MAXIMUM_CLIENTS + 2, 0);
}

void handle_new_client(int server_fd, client_t *clients) {
    bool connected = false;
    int client_fd = 0;
    int bytes_sent = 0;

    // Accept a client
    client_fd = accept4(server_fd, NULL, NULL, SOCK_CLOEXEC);
    if (-1 == client_fd) {
        perror("accept failed");
        return;
    }
    for (int i = 0; i < MAXIMUM_CLIENTS && !connected; i++) {
        if (NO_SOCKET != clients[i].client_fd) {
            continue;
        }

        // Found an empty client slot
        clients[i].client_fd = client_fd;
        connected = true;
    }

    if (!connected) {
        close(client_fd);
    }
    else
    {
        // Asks for the client's name
        bytes_sent = send(client_fd, WELCOME_BANNER, sizeof(WELCOME_BANNER) - 1, 0);
        if (bytes_sent != sizeof(WELCOME_BANNER) - 1)
        {
            perror("Failed to send welcome banner");
            exit(-1);
        }
    }
}

void handle_messages(client_t *clients, struct pollfd *poll_fds)
{
    for(int i=2; i < MAXIMUM_CLIENTS+2; i++)
    {
        if (NO_SOCKET != clients[i-2].client_fd && poll_fds[i].revents)
        {
            handle_client(clients, i-2);
        }
    }
}

void handle_broadcast(int broadcast_fd, int server_port)
{
    int bytes_recv = 0;
    struct sockaddr_in client_address = {0};
    socklen_t client_address_size = sizeof(client_address);
    char message[MAX_MESSAGE_SIZE] = {0};

    bytes_recv = recvfrom(broadcast_fd, message, sizeof(message), 0, &client_address, &client_address_size);
    switch(bytes_recv)
    {
        case -1:
            perror("Failed to read broadcast!");
            // We dont exit here because it could be a random broadcast
            return;
        case 0:
            // Connection error? anyways, UDP is best effort...
            return;
        default:
            break;
    }
    if (0 == strcmp(message, BROADCAST_DISCOVER_MESSAGE))
    {
        // We got a DISCOVER message, send respond...

        printf("Received Discover broadcast from %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        int bytes_to_send = snprintf(message, sizeof(message), "%s:%d", BROADCAST_ANSWER_MESSAGE, server_port);
        if (0 > bytes_to_send)
        {
            fprintf(stderr, "Error: failed on snprintf\n");
            return;
        }
        if (-1 == sendto(broadcast_fd, message, bytes_to_send, 0, &client_address, client_address_size))
        {
            perror("sendto failed"); // Best-effort
        }
    }
}

int main(int argc, char *argv[])
{
    int server_fd = 0;
    int poll_result = 0;
    int broadcast_fd = 0;
    int server_port = 0;
    client_t clients[MAXIMUM_CLIENTS];
    struct pollfd poll_fds[MAXIMUM_CLIENTS + 2]; // +2 for server and broadcast fds

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return -1;
    }

    server_port = atoi(argv[1]);
    if (0 >= server_port)
    {
        fprintf(stderr, "Error: Port must be greater than 0!\n");
        return -2;
    }

    // Initializes clients
    for(int i=0; i < MAXIMUM_CLIENTS; i++)
    {
        clients[i].client_fd = NO_SOCKET;
        memset(clients[i].name, 0, sizeof(clients[i].name));
    }

    // Setup the server (socket, bind, listen)
    server_fd = setup_server(server_port);

    // Setup the broadcast listener
    broadcast_fd = setup_broadcast();

    while(true)
    {
        poll_result = handle_poll(server_fd, broadcast_fd, clients, poll_fds);
        switch (poll_result)
        {
        case -1:
            perror("poll failed");
            exit(-1);
        case 0:
            // Timeout has occurred
            break;
        default:
            if (poll_fds[0].revents)
            {
                // A new client is connecting
                handle_new_client(server_fd, clients);
            }
            else if(poll_fds[1].revents)
            {
                // A broadcast was received
                handle_broadcast(broadcast_fd, server_port);
            }
            else
            {
                // One client or more sent a message
                handle_messages(clients, poll_fds);
            }
        }
    }
}

int setup_broadcast()
{
    struct sockaddr_in broadcast_address = {0};
    int broadcast_fd = 0;

    broadcast_fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (-1 == broadcast_fd)
    {
        perror("Failed to creat broadcast socket");
        exit(errno);
    }

    // Sets socket option SO_REUSEADDR, other processes will be able to listen on this port
    // This is a requirement if we want multiple chat servers on the same device
    int enabled = 1;
    if (0 != setsockopt(broadcast_fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)))
    {
        perror("Failed to set SO_REUSEADDR");
        exit(errno);
    }

    // Construct the server's data struct
    broadcast_address.sin_family = AF_INET;
    broadcast_address.sin_addr.s_addr = htonl(INADDR_ANY);  // Use all interfaces for BC
    broadcast_address.sin_port = htons(BROADCAST_PORT);     // The port to listen for BC

    // Bind socket to listen for broadcasts
    if (0 != bind(broadcast_fd, (struct sockaddr *)&broadcast_address, sizeof(broadcast_address)))
    {
        perror("Failed to bind");
        exit(errno);
    }

    printf("Listening for broadcasts on %d...\n", BROADCAST_PORT);

    return broadcast_fd;
}

int setup_server(int port)
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
    server_address.sin_port = htons(port);

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

    printf("Listening for incoming connections...\n");
    return server_fd;
}
