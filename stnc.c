#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <sys/un.h>
#include <netdb.h>
#include <time.h>
#include <sys/time.h>
#include <openssl/md5.h>
#include <openssl/evp.h>
#include <stdint.h>

#define SERVER_ADDR "2001:0db8:85a3:0000:0000:8a2e:0370:7334"
#define BUFFER_SIZE 1024
#define BUFSIZE (1024 * 1024 * 100) // 100MB buffer size

void run_performance_test(char *type, char *param, char *ip, uint16_t port);

void chat(int sockfd)
{
    char buffer[BUFFER_SIZE];
    int n;

    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO; // stdin
    fds[0].events = POLLIN;
    fds[1].fd = sockfd; // socket
    fds[1].events = POLLIN;
    while (1)
    {
        poll(fds, 2, -1); // wait indefinitely for events

        // check for stdin input
        if (fds[0].revents & POLLIN)
        {

            memset(buffer, 0, sizeof(buffer));
            fgets(buffer, sizeof(buffer), stdin);
            if (strcmp(buffer, "exit\n") == 0)
            {
                printf("Exiting...\n");
                exit(1);
                break;
            }
            char msg[BUFFER_SIZE + 8];
            sprintf(msg, "other: %s", buffer);
            n = send(sockfd, msg, strlen(msg), 0);
            if (n < 0)
            {
                printf("Send failed...\n");
                break;
            }
        }

        // check for socket input
        if (fds[1].revents & POLLIN)
        {
            memset(buffer, 0, sizeof(buffer));
            n = recv(sockfd, buffer, sizeof(buffer), 0);
            if (n < 0)
            {
                printf("Receive failed...\n");
                break;
            }
            printf("%s", buffer);
        }
    }
}

void client(const char *server_ip, const char *server_port)
{
    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        printf("Socket creation failed...\n");
        exit(1);
    }
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(server_ip);
    servaddr.sin_port = htons(atoi(server_port));
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
    {
        printf("Connection failed...\n");
        exit(1);
    }
    printf("Connected to server on port %s...\n", server_port);
    chat(sockfd);
    close(sockfd);
}

void server(const char *server_port)
{
    int sockfd, connfd;
    struct sockaddr_in servaddr, cli;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        printf("Socket creation failed...\n");
        exit(1);
    }

    // Add this block to make sure the socket is reused after closing
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(atoi(server_port));
    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
    {
        printf("Socket bind failed...\n");
        exit(1);
    }
    if (listen(sockfd, 5) != 0)
    {
        printf("Listen failed...\n");
        exit(1);
    }
    printf("Server listening on port %s...\n", server_port);

    while (1)
    {
        socklen_t cli_len = sizeof(cli);
        connfd = accept(sockfd, (struct sockaddr *)&cli, &cli_len);

        if (connfd < 0)
        {
            printf("Server accept failed...\n");
            exit(1);
        }
        printf("Server accepted a new client...\n");
        chat(connfd);
        close(connfd);
    }

    close(sockfd);
}

int main_stnc(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Usage:\n");
        printf("Client side: stnc -c IP PORT\n");
        printf("Server side: stnc -s PORT\n");
        return 1;
    }

    if (strcmp(argv[1], "-c") == 0)
    {
        const char *server_ip = argv[2];
        const char *server_port = argv[3];
        client(server_ip, server_port);
    }
    else if (strcmp(argv[1], "-s") == 0)
    {
        const char *server_port = argv[2];
        server(server_port);
    }
    else
    {
        printf("Invalid command\n");
        return 1;
    }

    return 0;
}
