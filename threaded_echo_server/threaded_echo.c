
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
#include <sys/types.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_INTERFACE    ("0.0.0.0")
#define SERVER_PORT         (12345)

#define MAX_MESSAGE_SIZE    (256)

int setup_server();

typedef struct client_s {
    int client_fd;
    struct sockaddr_in client_address;
} client_t;

void *handle_client(void *client_data)
{
    int bytes_recv = 0;
    int bytes_sent = 0;
    char message[MAX_MESSAGE_SIZE] = {0};
    char *client_ip = NULL;
    int client_port = 0;
    client_t *client = (client_t *)client_data;

    // First, register a cleanup function to release the client's allocated data.
    // Eliminating the risk of a memory leak
    pthread_cleanup_push(free, client);                           // will be called last
    pthread_cleanup_push(close, (void *)client->client_fd); // will be called first

    // Print client information
    client_ip = inet_ntoa(client->client_address.sin_addr);
    client_port = ntohs(client->client_address.sin_port);
    printf("A client connected from: %s:%d! handling thread: 0x%lx\n", client_ip, client_port, pthread_self());

    while(true)
    {
        // Receive client message
        bytes_recv = recv(client->client_fd, message, sizeof(message), 0);
        if (-1 == bytes_recv)
        {
            perror("recv failed");
            pthread_exit(NULL);
        }

        if (0 == bytes_recv)
        {
            // Connection probably closed...
            printf("client %s:%d disconnected.\n", client_ip, client_port);
            break;
        }

        // A message was received, print it.
        message[bytes_recv-1] = '\0'; // replacing new-line with null-terminator
        printf("Echoing '%s' to %s:%d...\n", message, client_ip, client_port);
        message[bytes_recv-1] = '\n'; // replacing null-terminator with new-line

        // Echoing the message...
        bytes_sent = send(client->client_fd, message, bytes_recv, 0);
        if (bytes_sent != bytes_recv)
        {
            perror("Failed to echo message");
            pthread_exit(NULL);
        }
    }

    // Cleanup
    pthread_cleanup_pop(true);
    pthread_cleanup_pop(true);
    // Exiting the thread
    printf("Exiting thread: 0x%lx\n", pthread_self());
    pthread_exit(NULL);
}

int main()
{
    int server_fd = 0;
    client_t *client = NULL;
    socklen_t client_address_size = sizeof(client->client_address);
    pthread_t tid;

    // Setup the server (socket, bind, listen)
    server_fd = setup_server();

    // Keep the server running to accept new clients
    while (true)
    {
        // Allocate memory for client data
        client = malloc(sizeof(*client));
        if (NULL == client)
        {
            perror("malloc failed");
            exit(errno);
        }

        // Accept a client
        client->client_fd = accept4(server_fd, (struct sockaddr *)&client->client_address,
                                    &client_address_size, SOCK_CLOEXEC);
        if (0 < client->client_fd)
        {
            // Creates a new thread to handle the client
            errno = pthread_create(&tid, NULL, handle_client, (void *)client);
            if (0 != errno)
            {
                perror("Error while creating thread 1");
                exit(errno);
            }

            // Detach the client's thread, so we don't need to keep it's information.
            // Detaching the thread allows us not to call join,
            // while making sure the thread will be reaped appropriately.
            errno = pthread_detach(tid);
            if (0 != errno)
            {
                perror("Failed to detach thread");
                exit(errno);
            }
        }
        else
        {
            // Free the malloc'd data in-case accept failed
            free(client);
        }
    }

    free(client);
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
