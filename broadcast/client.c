
/**
 ** Written by Amit Sides
 **/

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <termios.h>
#include <errno.h>
#include <poll.h>

#include "common.h"

#define SERVER_IP  ("127.0.0.1")

#define USAGE       ("%s <name>\n")
#define ERASE_LINE  ("\33[2K\r")

#define BROADCAST_TIMEOUT (1)

char input_buffer[MAX_MESSAGE_SIZE + 1] = {0};
int input_index = 0;

void print_buffer()
{
    int i=0;
    printf("Message: ");

    for (; i < input_index; i++)
    {
        printf("%c", input_buffer[i]);
    }
}

void reset_terminal()
{
    struct termios t = {0};
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag |= ECHO;
    t.c_lflag |= ICANON;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    setvbuf(stdin, NULL, _IOLBF, 0);
    setvbuf(stdout, NULL, _IOLBF, 0);
}

void configure_terminal()
{
    struct termios t = {0};

    atexit(reset_terminal);

    if (0 != tcgetattr(STDIN_FILENO, &t))
    {
        perror("tcgetattr failed");
        exit(-2);
    }
    t.c_lflag &= ~ECHO;
    t.c_lflag &= ~ICANON;
    if (0 != tcsetattr(STDIN_FILENO, TCSANOW, &t))
    {
        perror("tcsetattr failed");
        exit(-3);
    }

    if (0 != setvbuf(stdin, NULL, _IONBF, 0))
    {
        perror("stdin setvbuf failed!");
        exit(-4);
    }
    if (0 != setvbuf(stdout, NULL, _IONBF, 0))
    {
        perror("stdout setvbuf failed!");
        exit(-5);
    }
}

int setup_connection(char *ip, int port)
{
    int socket_fd = 0;
    struct sockaddr_in server_address = {0};

    // Creates the socket
    socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (-1 == socket_fd)
    {
        perror("socket failed");
        exit(errno);
    }

    // Constructs the server address struct
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);

    if (1 != inet_pton(AF_INET, ip, &server_address.sin_addr))
    {
        perror("inet_pton failed");
        exit(errno);
    }

    printf("Connecting to %s:%d\n", inet_ntoa(server_address.sin_addr), port);

    // Connects to the server
    if (0 != connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)))
    {
        perror("connect failed");
        exit(errno);
    }

    return socket_fd;
}

void handle_character_input(int socket_fd)
{
    unsigned character = 0;

    character = getchar();
    switch(character)
    {
        case 127: // backspace
            if (input_index <= 0)
            {
                return;
            }
            input_index--;
            input_buffer[input_index] = '\0';
            break;
        case '\n':
            // Check buffer is not empty
            if (input_index <= 0)
            {
                return;
            }

            // Send current message
            input_buffer[input_index++] = character;
            if (-1 == send(socket_fd, input_buffer, input_index, 0))
            {
                printf(ERASE_LINE);
                perror("send failed");
                exit(errno);
            }
            input_index = 0;
            break;
        default:
            if (input_index >= MAX_MESSAGE_SIZE - 1)
            {
                // buffer is full, do nothing
                return;
            }
            input_buffer[input_index++] = character;
    }

    printf(ERASE_LINE);
    print_buffer();
}

void handle_message(int socket_fd)
{
    int bytes_recv = 0;
    char buffer[MAX_MESSAGE_SIZE] = {0};

    bytes_recv = recv(socket_fd, buffer, sizeof(buffer), 0);
    switch(bytes_recv)
    {
        case -1:
            printf(ERASE_LINE);
            perror("recv failed");
            exit(errno);
        case 0:
            printf(ERASE_LINE);
            fprintf(stderr, "Connection closed :(\n");
            exit(errno);
        default:
            break;
    }

    printf(ERASE_LINE);
    printf("%s", buffer);
    print_buffer();
}

void handle_client_connection(int socket_fd)
{
    struct pollfd pollfds[2] = {0};
    int poll_result = 0;

    // Initializes pollfd structs
    pollfds[0].fd = STDIN_FILENO;
    pollfds[0].events = POLLIN;
    pollfds[1].fd = socket_fd;
    pollfds[1].events = POLLIN;

    poll_result = poll(pollfds, sizeof(pollfds) / sizeof(*pollfds), 0);
    switch(poll_result)
    {
        case -1:
            // Error occurred
            printf(ERASE_LINE);
            perror("poll failed");
            exit(errno);
        case 0:
            // timeout occurred, just exit
            return;
        default:
            break;
    }

    // First, check for an error
    if ((pollfds[0].revents & POLLHUP) || (pollfds[0].revents & POLLERR) || (pollfds[0].revents & POLLNVAL) ||
        (pollfds[1].revents & POLLHUP) || (pollfds[1].revents & POLLERR) || (pollfds[1].revents & POLLNVAL))
    {
        // One of the file descriptors got an error
        printf(ERASE_LINE);
        fprintf(stderr, "A file descriptor got an error :(\n");
        exit(-1);
    }

    // Either we got a message, or an input from stdin, or both
    if (pollfds[0].revents & POLLIN)
    {
        // Input from stdin
        handle_character_input(socket_fd);
    }
    if (pollfds[1].revents & POLLIN)
    {
        // message from socket
        handle_message(socket_fd);
    }
}

int send_broadcast(int port, char *message, size_t message_size)
{
    struct sockaddr_in broadcast_address = {0};

    int sender_fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (-1 == sender_fd)
    {
        perror("socket");
        exit(errno);
    }

    int enabled = 1;
    if (0 != setsockopt(sender_fd, SOL_SOCKET, SO_BROADCAST, &enabled, sizeof(enabled)))
    {
        perror("Failed to set SO_REUSEADDR");
        exit(errno);
    }

    broadcast_address.sin_family = AF_INET;
    broadcast_address.sin_port=htons(port);
    broadcast_address.sin_addr.s_addr=htonl(INADDR_BROADCAST);

    // Best Effort
    sendto(sender_fd, message, message_size, 0,
           (struct sockaddr *)&broadcast_address, sizeof(broadcast_address));

    return sender_fd;
}

int get_server(char *ip)
{
    char buffer[MAX_MESSAGE_SIZE] = {0};
    struct sockaddr_in server_address = {0};
    socklen_t server_address_size = sizeof(server_address);
    char **ips = NULL;
    int *ports = NULL;
    int counter = 0;
    int bytes_recv = 0;
    int server_index = 0;
    int server_port = 0;

    int broadcast_fd = send_broadcast(BROADCAST_PORT, BROADCAST_DISCOVER_MESSAGE, sizeof(BROADCAST_DISCOVER_MESSAGE));

    printf("Searching for servers...\n");
    sleep(BROADCAST_TIMEOUT);
    do
    {
        bytes_recv = recvfrom(broadcast_fd, buffer, sizeof(buffer), MSG_DONTWAIT,
                              (struct sockaddr *)&server_address, &server_address_size);
        if (0 < bytes_recv)
        {
            // We received an answer!
            if (0 != strncmp(buffer, BROADCAST_ANSWER_MESSAGE, sizeof(BROADCAST_ANSWER_MESSAGE)-1))
            {
                // Invalid answer :(
                continue;
            }

            // The answer is in the format of "ANSWER:<port>"

            // Increase ips & ports tables
            counter++;
            ips = realloc(ips, counter * sizeof(*ips));
            ports = realloc(ports, counter * sizeof(*ports));
            if (NULL == ips || NULL == ports)
            {
                perror("realloc failed");
                exit(errno);
            }
            ips[counter-1] = malloc(IPV4_SIZE);
            if (NULL == ips[counter-1])
            {
                perror("malloc failed");
                exit(errno);
            }


            ports[counter-1] = atoi(&buffer[sizeof(BROADCAST_ANSWER_MESSAGE)]);
            if (0 == ports[counter-1] ||
                NULL == inet_ntop(AF_INET, &server_address.sin_addr, ips[counter-1], IPV4_SIZE))
            {
                perror("inet_ntop failed");
                counter--;
                free(ips[counter]);
            }
        }
    } while(EWOULDBLOCK != errno && EAGAIN != errno && -1 != bytes_recv);

    // Displaying answers to the user and let him choose server
    printf("\nFound chat servers:\n");
    for(int i=0; i<counter; i++)
    {
        printf("%d. %s:%d\n", i+1, ips[i], ports[i]);
    }
    do
    {
        printf("Select a server: ");
        fflush(stdout);
        scanf("%d", &server_index);
    } while(server_index<=0 || server_index>counter);

    // Saving selected server data
    memcpy(ip, ips[server_index-1], IPV4_SIZE);
    server_port = ports[server_index-1];

    // Freeing all the malloc'd data
    for(int i=0; i<counter; i++)
    {
        free(ips[i]);
    }
    free(ips);
    free(ports);
    return server_port;
}

int main(int argc, char *argv[])
{
    int socket_fd = 0;
    char ip[IPV4_SIZE] = {0};
    int port = 0;
    if (argc < 2)
    {
        printf("Not enough arguments.\n");
        printf(USAGE, argv[0]);
        return -1;
    }

    if (MAX_NAME_SIZE <=strlen(argv[1]))
    {
        printf("Name '%s' is too long. Use less than %d characters.\n", argv[1], MAX_NAME_SIZE);
        printf(USAGE, argv[0]);
        return -1;
    }

    port = get_server(ip);

    configure_terminal();

    socket_fd = setup_connection(ip, port);

    // Sends the server the name
    printf("%s", argv[1]);
    if (-1 == send(socket_fd, argv[1], strlen(argv[1])+1, 0))
    {
        perror("Failed to send name");
        exit(errno);
    }

    while(true)
    {
        handle_client_connection(socket_fd);
    }
}