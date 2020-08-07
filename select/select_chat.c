
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
#include <sys/types.h>
#include <arpa/inet.h>

#include "common.h"
#include "colors.h"

#define NO_SOCKET           (-1)
#define SERVER_INTERFACE    ("0.0.0.0")
#define MAXIMUM_CLIENTS     (sizeof(colors) / sizeof(*colors)) // = 6

#define WELCOME_BANNER      ("Hello! Please enter your name: ")

int setup_server();

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
            exit(bytes_sent);
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

int handle_select(int server_fd, client_t *clients, fd_set *read_fds)
{
    int maximum_fd = server_fd;

    // Initializing the fd_set
    FD_ZERO(read_fds);

    // Setting the bits of the relevant fds
    FD_SET(server_fd, read_fds);
    for(int i=0; i < MAXIMUM_CLIENTS; i++)
    {
        if (NO_SOCKET != clients[i].client_fd)
        {
            FD_SET(clients[i].client_fd, read_fds);
        }
    }

    // Calculating the maximum fd (needed for the call to select)
    for(int i=0; i < MAXIMUM_CLIENTS; i++)
    {
        if (NO_SOCKET != clients[i].client_fd && maximum_fd < clients[i].client_fd)
        {
            maximum_fd = clients[i].client_fd;
        }
    }

    // Calling select with the set
    return select(maximum_fd + 1, read_fds, NULL, NULL, NULL);
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

void handle_messages(client_t *clients, fd_set *read_fds)
{
    for(int i=0; i < MAXIMUM_CLIENTS; i++)
    {
        if (NO_SOCKET != clients[i].client_fd && FD_ISSET(clients[i].client_fd, read_fds))
        {
            handle_client(clients, i);
        }
    }
}

int main()
{
    int server_fd = 0;
    int select_result = 0;
    client_t clients[MAXIMUM_CLIENTS];
    fd_set read_fds;

    // Initializes clients
    for(int i=0; i < MAXIMUM_CLIENTS; i++)
    {
        clients[i].client_fd = NO_SOCKET;
        memset(clients[i].name, 0, sizeof(clients[i].name));
    }

    // Setup the server (socket, bind, listen)
    server_fd = setup_server();

    while(true)
    {
        select_result = handle_select(server_fd, clients, &read_fds);
        switch (select_result)
        {
        case -1:
            perror("select failed");
            exit(-1);
        case 0:
            // Timeout has occurred
            break;
        default:
            if (FD_ISSET(server_fd, &read_fds))
            {
                // A new client is connecting
                handle_new_client(server_fd, clients);
            }
            else
            {
                // One client or more sent a message
                handle_messages(clients, &read_fds);
            }
        }
    }
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

    printf("Listening for incoming connections...\n");
    return server_fd;
}
