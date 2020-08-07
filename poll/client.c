
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

int setup_connection()
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
    server_address.sin_port = htons(SERVER_PORT);

    if (1 != inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr))
    {
        perror("inet_pton failed");
        exit(errno);
    }

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

int main(int argc, char *argv[])
{
    int socket_fd = 0;
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

    configure_terminal();

    socket_fd = setup_connection();

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