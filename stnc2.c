#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/mman.h> // For mmap, munmap
#include <sys/poll.h>

#define BUF_SIZE 128
#define CHUNK_SIZE 1472   // Adjusted chunk size
#define BUFSIZE 104857600 // 100 MB
#define BUFFER_SIZE 1024
#define ERROR -1

void generate_data(char *data, int size)
{
    // Seed the random number generator
    srand(time(NULL));

    // Generate random data
    for (int i = 0; i < size; i++)
    {
        data[i] = rand() % 256;
    }
}

unsigned long generate_checksum(char *data)
{
    unsigned long sum = 0;
    for (int i = 0; i < BUFSIZE; i++)
    {
        sum += (unsigned char)data[i];
    }
    return sum;
}

int send_data_tcp_ipv4(const char *ip_addr, int port)
{
    clock_t start_time = clock();
    char *data = (char *)malloc(BUFSIZE);
    if (data == NULL)
    {
        perror("Failed to allocate memory");
        return ERROR;
    }
    generate_data(data, BUFSIZE);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket creation failed");
        free(data);
        return ERROR;
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip_addr, &servaddr.sin_addr) <= 0)
    {
        perror("inet_pton error");
        close(sockfd);
        free(data);
        return ERROR;
    }

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("connect failed");
        close(sockfd);
        free(data);
        return ERROR;
    }

    unsigned long checksum = generate_checksum(data);

    ssize_t total_sent = 0, sent = 0;
    while (total_sent < BUFSIZE)
    {
        sent = write(sockfd, data + total_sent, BUFSIZE - total_sent);
        if (sent < 0)
        {
            perror("write failed");
            close(sockfd);
            free(data);
            return ERROR;
        }
        total_sent += sent;
    }

    // send the checksum
    char checksum_str[20];
    sprintf(checksum_str, "%lu", checksum);
    write(sockfd, checksum_str, sizeof(checksum_str));

    clock_t end_time = clock();

    close(sockfd);
    free(data);

    return (int)(end_time - start_time) * 1000 / CLOCKS_PER_SEC;
}

void receive_data_tcp_ipv4(int port)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket creation failed");
        return;
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("bind failed");
        close(sockfd);
        return;
    }

    if (listen(sockfd, 1) < 0)
    {
        perror("listen failed");
        close(sockfd);
        return;
    }

    printf("Listening on port: %d\n", port);

    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);
    int connfd = accept(sockfd, (struct sockaddr *)&cliaddr, &len);
    if (connfd < 0)
    {
        perror("accept failed");
        close(sockfd);
        return;
    }

    char *data = (char *)malloc(BUFSIZE);
    if (data == NULL)
    {
        perror("Failed to allocate memory");
        close(connfd);
        close(sockfd);
        return;
    }

    // Start timer
    clock_t start_time = clock();

    ssize_t total_received = 0, received = 0;
    while (total_received < BUFSIZE)
    {
        received = read(connfd, data + total_received, BUFSIZE - total_received);
        if (received < 0)
        {
            perror("read failed");
            close(connfd);
            close(sockfd);
            free(data);
            return;
        }
        else if (received == 0)
        {
            printf("Connection closed by client\n");
            break;
        }
        total_received += received;
    }

    char checksum_str[20];
    read(connfd, checksum_str, sizeof(checksum_str));
    unsigned long received_checksum = strtoul(checksum_str, NULL, 10);

    // Stop timer
    clock_t end_time = clock();

    // Calculate elapsed time in milliseconds
    int elapsed_time = (int)(end_time - start_time) * 1000 / CLOCKS_PER_SEC;

    // Check data integrity
    if (total_received == BUFSIZE)
    {
        unsigned long checksum = generate_checksum(data);
        if (checksum == received_checksum)
        {
            printf("Data integrity check passed.\n");
        }
        else
        {
            printf("Data integrity check failed.\n");
        }
    }
    else
    {
        printf("Data size mismatch. Data integrity check failed.\n");
    }

    close(connfd);
    close(sockfd);
    free(data);

    printf("ipv4_tcp, %d \n", elapsed_time);
}
void send_data_udp_ipv4(const char *ip_addr, int port)
{
    // Create socket and set server address
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip_addr, &servaddr.sin_addr) <= 0)
    {
        perror("inet_pton error");
        exit(EXIT_FAILURE);
    }

    // Generate data
    char *data = (char *)malloc(BUFSIZE);
    if (data == NULL)
    {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    generate_data(data, BUFSIZE);

    // Start timer
    clock_t start_time = clock();

    // Send packets
    int num_packets = BUFSIZE / CHUNK_SIZE;
    int remaining_bytes = BUFSIZE % CHUNK_SIZE;
    int total_sent = 0;

    // Send full packets
    for (int i = 0; i < num_packets; i++)
    {
        int num_bytes_sent = sendto(sockfd, data + (i * CHUNK_SIZE), CHUNK_SIZE, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
        if (num_bytes_sent < 0)
        {
            perror("sendto failed");
            exit(EXIT_FAILURE);
        }
        total_sent += num_bytes_sent;
    }

    // Send remaining bytes
    if (remaining_bytes > 0)
    {
        int num_bytes_sent = sendto(sockfd, data + (num_packets * CHUNK_SIZE), remaining_bytes, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
        if (num_bytes_sent < 0)
        {
            perror("sendto failed");
            exit(EXIT_FAILURE);
        }
        total_sent += num_bytes_sent;
    }
    // Stop timer
    clock_t end_time = clock();

    // Calculate elapsed time in milliseconds
    int elapsed_time = (int)(end_time - start_time) * 1000 / CLOCKS_PER_SEC;

    printf("Total Sent: %d bytes\n", total_sent);
    printf("Elapsed Time: %d ms\n", elapsed_time);

    close(sockfd);
    free(data);
}

void receive_data_udp_ipv4(int port)
{

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in servaddr, cliaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port: %d\n", port);

    char *data = (char *)malloc(BUFSIZE);
    if (data == NULL)
    {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    socklen_t len = sizeof(cliaddr);
    ssize_t total_received = 0, received = CHUNK_SIZE;

    // Start timer
    clock_t start_time = clock();

    while (received == CHUNK_SIZE) // Keep receiving until we get less than CHUNK_SIZE
    {
        received = recvfrom(sockfd, data + total_received, CHUNK_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
        if (received < 0)
        {
            perror("recvfrom failed");
            exit(EXIT_FAILURE);
        }
        total_received += received;
    }

    // Stop timer
    clock_t end_time = clock();

    // Calculate elapsed time in milliseconds
    int elapsed_time = (int)(end_time - start_time) * 1000 / CLOCKS_PER_SEC;

    close(sockfd);
    printf("ipv4_udp, %d \n", elapsed_time);
}

int send_data_tcp_ipv6(const char *ip_addr, int port)
{
    clock_t start_time = clock();
    char *data = (char *)malloc(BUFSIZE);
    if (data == NULL)
    {
        perror("Failed to allocate memory");
        return ERROR;
    }
    generate_data(data, BUFSIZE);

    int sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in6 servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin6_family = AF_INET6;
    servaddr.sin6_port = htons(port);
    if (inet_pton(AF_INET6, ip_addr, &servaddr.sin6_addr) <= 0)
    {
        printf("IP Address: %s\n", ip_addr);
        perror("inet_pton error");
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        printf("IP Address: %s\n", ip_addr);
        printf("Port: %d\n", port);
        printf("Errno: %d\n", errno);
        printf("Error Message: %s\n", strerror(errno));
        perror("connect failed");
        exit(EXIT_FAILURE);
    }

    unsigned long checksum = generate_checksum(data);

    ssize_t total_sent = 0, sent = 0;
    while (total_sent < BUFSIZE)
    {
        sent = write(sockfd, data + total_sent, BUFSIZE - total_sent);
        if (sent < 0)
        {
            perror("write failed");
            close(sockfd);
            free(data);
            return ERROR;
        }
        total_sent += sent;
    }

    // send the checksum
    char checksum_str[20];
    sprintf(checksum_str, "%lu", checksum);
    write(sockfd, checksum_str, sizeof(checksum_str));

    clock_t end_time = clock();

    close(sockfd);
    free(data);

    return (int)(end_time - start_time) * 1000 / CLOCKS_PER_SEC;
}

void receive_data_tcp_ipv6(int port)
{
    int sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in6 servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin6_family = AF_INET6;
    servaddr.sin6_addr = in6addr_any; // This allows the server to listen on all IPv6 addresses
    servaddr.sin6_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, 1) < 0)
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port: %d\n", port);

    struct sockaddr_in6 cliaddr;
    socklen_t len = sizeof(cliaddr);
    int connfd = accept(sockfd, (struct sockaddr *)&cliaddr, &len);
    if (connfd < 0)
    {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }

    char *data = (char *)malloc(BUFSIZE);
    if (data == NULL)
    {
        perror("Failed to allocate memory");
        close(connfd);
        close(sockfd);
        return;
    }

    // Start timer
    clock_t start_time = clock();

    ssize_t total_received = 0, received = 0;
    while (total_received < BUFSIZE)
    {
        received = read(connfd, data + total_received, BUFSIZE - total_received);
        if (received < 0)
        {
            perror("read failed");
            close(connfd);
            close(sockfd);
            free(data);
            return;
        }
        else if (received == 0)
        {
            printf("Connection closed by client\n");
            break;
        }
        total_received += received;
    }

    char checksum_str[20];
    read(connfd, checksum_str, sizeof(checksum_str));
    unsigned long received_checksum = strtoul(checksum_str, NULL, 10);

    // Stop timer
    clock_t end_time = clock();

    // Calculate elapsed time in milliseconds
    int elapsed_time = (int)(end_time - start_time) * 1000 / CLOCKS_PER_SEC;

    // Check data integrity
    if (total_received == BUFSIZE)
    {
        unsigned long checksum = generate_checksum(data);
        if (checksum != received_checksum)
        {
            printf("Data integrity check failed.\n");
        }
    }
    else
    {
        printf("Data size mismatch. Data integrity check failed.\n");
    }

    close(connfd);
    close(sockfd);
    free(data);

    printf("ipv6_tcp, %d \n", elapsed_time);
}
void send_data_udp_ipv6(const char *ip_addr, int port)
{
    // Create socket and set server address
    int sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in6 servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin6_family = AF_INET6;
    servaddr.sin6_port = htons(port);
    if (inet_pton(AF_INET6, ip_addr, &servaddr.sin6_addr) <= 0)
    {
        perror("inet_pton error");
        exit(EXIT_FAILURE);
    }

    // Generate data
    char *data = (char *)malloc(BUFSIZE);
    if (data == NULL)
    {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    generate_data(data, BUFSIZE);

    // Start timer
    clock_t start_time = clock();

    // Send packets
    int num_packets = BUFSIZE / CHUNK_SIZE;
    int remaining_bytes = BUFSIZE % CHUNK_SIZE;
    int total_sent = 0;

    // Send full packets
    for (int i = 0; i < num_packets; i++)
    {
        int num_bytes_sent = sendto(sockfd, data + (i * CHUNK_SIZE), CHUNK_SIZE, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
        if (num_bytes_sent < 0)
        {
            perror("sendto failed");
            exit(EXIT_FAILURE);
        }
        total_sent += num_bytes_sent;
    }

    // Send remaining bytes
    if (remaining_bytes > 0)
    {
        int num_bytes_sent = sendto(sockfd, data + (num_packets * CHUNK_SIZE), remaining_bytes, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
        if (num_bytes_sent < 0)
        {
            perror("sendto failed");
            exit(EXIT_FAILURE);
        }
        total_sent += num_bytes_sent;
    }
    // Stop timer
    clock_t end_time = clock();

    // Calculate elapsed time in milliseconds
    int elapsed_time = (int)(end_time - start_time) * 1000 / CLOCKS_PER_SEC;

    close(sockfd);
    free(data);
    printf("Total sent: %d bytes\n", total_sent);
    printf("Elapsed Time: %d ms\n", elapsed_time);
}

void receive_data_udp_ipv6(int port)
{
    int sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in6 servaddr, cliaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin6_family = AF_INET6;
    servaddr.sin6_addr = in6addr_any; // This allows the server to listen on all IPv6 addresses
    servaddr.sin6_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port: %d\n", port);

    char *data = (char *)malloc(BUFSIZE);
    if (data == NULL)
    {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    socklen_t len = sizeof(cliaddr);
    ssize_t total_received = 0, received = CHUNK_SIZE;

    // Start timer
    clock_t start_time = clock();

    while (received == CHUNK_SIZE) // Keep receiving until we get less than CHUNK_SIZE
    {
        received = recvfrom(sockfd, data + total_received, CHUNK_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
        if (received < 0)
        {
            perror("recvfrom failed");
            exit(EXIT_FAILURE);
        }
        total_received += received;
    }

    // Stop timer
    clock_t end_time = clock();

    // Calculate elapsed time in milliseconds
    int elapsed_time = (int)(end_time - start_time) * 1000 / CLOCKS_PER_SEC;

    close(sockfd);
    free(data);

    printf("ipv6_udp, %d \n", elapsed_time);
}

void send_data_uds_dgram(const char *sock_path)
{
    int sockfd;
    struct sockaddr_un addr;
    clock_t start_time = clock();

    if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    char *data = (char *)malloc(CHUNK_SIZE);
    if (data == NULL)
    {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    generate_data(data, CHUNK_SIZE);
    // Send data in chunks
    for (int i = 0; i < CHUNK_SIZE; i += BUFFER_SIZE)
    {
        int to_send = CHUNK_SIZE - i;
        if (to_send > BUFFER_SIZE)
        {
            to_send = BUFFER_SIZE;
        }

        if (sendto(sockfd, data + i, to_send, 0, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            perror("sendto error");
            exit(EXIT_FAILURE);
        }
    }

    clock_t end_time = clock();
    double time_taken = ((double)end_time - start_time) / CLOCKS_PER_SEC * 1000;

    printf("uds_dgram,%f\n", time_taken);

    close(sockfd);
}

void receive_data_uds_dgram(const char *sock_path)
{
    int sockfd;
    struct sockaddr_un addr;
    clock_t start_time = clock();

    if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    unlink(sock_path);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind error");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Socket bound successfully to %s\n", sock_path); // Add this line
    }

    char *data = (char *)malloc(CHUNK_SIZE);
    if (data == NULL)
    {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    // Receive data in chunks
    int total_received = 0;
    while (total_received < CHUNK_SIZE)
    {
        int to_receive = CHUNK_SIZE - total_received;
        if (to_receive > BUFFER_SIZE)
        {
            to_receive = BUFFER_SIZE;
        }

        int received = recvfrom(sockfd, data + total_received, to_receive, 0, NULL, NULL);
        if (received < 0)
        {
            perror("recvfrom error");
            exit(EXIT_FAILURE);
        }

        total_received += received;
    }

    clock_t end_time = clock();
    double time_taken = ((double)end_time - start_time) / CLOCKS_PER_SEC * 1000;

    printf("uds_dgram,%f\n", time_taken);

    close(sockfd);
}

void send_data_uds_stream(const char *sock_path)
{
    int sockfd;
    struct sockaddr_un addr;
    clock_t start_time = clock();

    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect error");
        exit(EXIT_FAILURE);
    }

    char *data = (char *)malloc(CHUNK_SIZE);
    if (data == NULL)
    {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    generate_data(data, CHUNK_SIZE);

    // Send data in chunks
    for (int i = 0; i < CHUNK_SIZE; i += BUFFER_SIZE)
    {
        int to_send = CHUNK_SIZE - i;
        if (to_send > BUFFER_SIZE)
        {
            to_send = BUFFER_SIZE;
        }

        if (send(sockfd, data + i, to_send, 0) < 0)
        {
            perror("send error");
            exit(EXIT_FAILURE);
        }
    }

    clock_t end_time = clock();
    double time_taken = ((double)end_time - start_time) / CLOCKS_PER_SEC * 1000;

    printf("uds_stream,%f\n", time_taken);

    close(sockfd);
}

void receive_data_uds_stream(const char *sock_path)
{
    int sockfd, newsockfd;
    struct sockaddr_un addr;
    socklen_t addrlen;
    clock_t start_time = clock();

    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    unlink(sock_path);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind error");
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, 5) < 0)
    {
        perror("listen error");
        exit(EXIT_FAILURE);
    }

    addrlen = sizeof(addr);
    if ((newsockfd = accept(sockfd, (struct sockaddr *)&addr, &addrlen)) < 0)
    {
        perror("accept error");
        exit(EXIT_FAILURE);
    }

    char *data = (char *)malloc(CHUNK_SIZE);
    if (data == NULL)
    {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    // Receive data in chunks
    int total_received = 0;
    while (total_received < CHUNK_SIZE)
    {
        int to_receive = CHUNK_SIZE - total_received;
        if (to_receive > BUFFER_SIZE)
        {
            to_receive = BUFFER_SIZE;
        }
        int received = recv(newsockfd, data + total_received, to_receive, 0);
        if (received < 0)
        {
            perror("recv error");
            exit(EXIT_FAILURE);
        }

        total_received += received;
    }

    clock_t end_time = clock();
    double time_taken = ((double)end_time - start_time) / CLOCKS_PER_SEC * 1000;

    printf("uds_stream,%f\n", time_taken);

    close(newsockfd);
    close(sockfd);
}

void send_data_mmap(const char *sock_path, const char *filename, const char *communication_style)
{
    int fd, sockfd;
    char *data;
    struct sockaddr_un addr;

    // Generate data
    data = malloc(BUFSIZE);
    if (data == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    generate_data(data, BUFSIZE);

    // Open file
    fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // Write data to file
    if (write(fd, data, BUFSIZE) != BUFSIZE)
    {
        perror("write");
        exit(EXIT_FAILURE);
    }

    // Memory map the file
    char *mmapped_data = mmap(0, BUFSIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (mmapped_data == MAP_FAILED)
    {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // Create a socket
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect error");
        exit(EXIT_FAILURE);
    }

    // Send data size
    int data_size = BUFSIZE;
    if (send(sockfd, &data_size, sizeof(data_size), 0) < 0)
    {
        perror("send error");
        exit(EXIT_FAILURE);
    }

    // Start timer
    clock_t start_time = clock();

    // Send data in chunks
    for (int i = 0; i < BUFSIZE; i += BUFFER_SIZE)
    {
        int to_send = BUFSIZE - i;
        if (to_send > BUFFER_SIZE)
        {
            to_send = BUFFER_SIZE;
        }

        if (send(sockfd, mmapped_data + i, to_send, 0) < 0)
        {
            perror("send error");
            exit(EXIT_FAILURE);
        }
    }

    // End timer
    clock_t end_time = clock();

    if (munmap(mmapped_data, BUFSIZE) == -1)
    {
        perror("munmap");
        exit(EXIT_FAILURE);
    }

    close(fd);
    close(sockfd);
    free(data);

    double time_taken = ((double)end_time - start_time) / CLOCKS_PER_SEC * 1000;
    printf("%s,%f\n", communication_style, time_taken);
}

void receive_data_mmap(const char *sock_path, const char *filename, const char *communication_style)
{
    int fd, sockfd, newsockfd;
    char *data;
    struct sockaddr_un addr;
    socklen_t addrlen;

    // Create a socket
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    unlink(sock_path);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind error");
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, 5) < 0)
    {
        perror("listen error");
        exit(EXIT_FAILURE);
    }
    addrlen = sizeof(addr);
    if ((newsockfd = accept(sockfd, (struct sockaddr *)&addr, &addrlen)) < 0)
    {
        perror("accept error");
        exit(EXIT_FAILURE);
    }

    // Receive data size
    int data_size;
    int received = recv(newsockfd, &data_size, sizeof(data_size), 0);
    if (received < 0)
    {
        perror("recv error");
        exit(EXIT_FAILURE);
    }

    // Allocate memory to receive data
    data = malloc(data_size);
    if (data == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // Receive data in chunks
    int total_received = 0;

    // Start timer
    clock_t start_time = clock();

    while (total_received < data_size)
    {
        int to_receive = data_size - total_received;
        if (to_receive > BUFFER_SIZE)
        {
            to_receive = BUFFER_SIZE;
        }

        received = recv(newsockfd, data + total_received, to_receive, 0);
        if (received < 0)
        {
            perror("recv error");
            exit(EXIT_FAILURE);
        }

        total_received += received;
    }

    // End timer
    clock_t end_time = clock();

    // Write data to file
    fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    if (write(fd, data, data_size) != data_size)
    {
        perror("write");
        exit(EXIT_FAILURE);
    }

    close(fd);
    close(newsockfd);
    close(sockfd);
    free(data);

    double time_taken = ((double)end_time - start_time) / CLOCKS_PER_SEC * 1000;
    printf("%s,%f\n", communication_style, time_taken);
}

void send_data_pipe(const char *pipe_path)
{
    int fd;
    clock_t start_time = clock();

    if ((fd = open(pipe_path, O_WRONLY)) < 0)
    {
        perror("pipe open error");
        exit(EXIT_FAILURE);
    }

    char *data = (char *)malloc(CHUNK_SIZE);
    if (data == NULL)
    {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    generate_data(data, CHUNK_SIZE);

    // Send data in chunks
    for (int i = 0; i < CHUNK_SIZE; i += BUF_SIZE)
    {
        int to_send = CHUNK_SIZE - i;
        if (to_send > BUF_SIZE)
        {
            to_send = BUF_SIZE;
        }

        if (write(fd, data + i, to_send) < 0)
        {
            perror("write error");
            exit(EXIT_FAILURE);
        }
    }

    free(data);

    clock_t end_time = clock();
    double time_taken = ((double)end_time - start_time) / CLOCKS_PER_SEC * 1000;

    printf("pipe,%f\n", time_taken);

    close(fd);
}

void receive_data_pipe(const char *pipe_path)
{
    int fd;
    clock_t start_time = clock();

    if ((fd = open(pipe_path, O_RDONLY)) < 0)
    {
        perror("pipe open error");
        exit(EXIT_FAILURE);
    }

    char *data = (char *)malloc(CHUNK_SIZE);
    if (data == NULL)
    {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    // Receive data in chunks
    int total_received = 0;
    while (total_received < CHUNK_SIZE)
    {
        int to_receive = CHUNK_SIZE - total_received;
        if (to_receive > BUF_SIZE)
        {
            to_receive = BUF_SIZE;
        }

        int received = read(fd, data + total_received, to_receive);
        if (received <= 0)
        {
            perror("read error");
            exit(EXIT_FAILURE);
        }

        total_received += received;
    }

    free(data);

    clock_t end_time = clock();
    double time_taken = ((double)end_time - start_time) / CLOCKS_PER_SEC * 1000;

    printf("pipe,%f\n", time_taken);

    close(fd);
}

void stnc2_client(const char *ip, int port, const char *type, const char *param)
{
    // Create a TCP socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set the server's IP address and port
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(ip);
    server_address.sin_port = htons(port);

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        perror("connect");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // Send type and param to the server
    char type_param[256];
    snprintf(type_param, sizeof(type_param), "%s:%s", type, param);
    send(client_socket, type_param, strlen(type_param), 0); // Add error checking

    // Close the socket

    if (type != NULL && param != NULL)
    {
        if (strcmp(type, "ipv4") == 0)
        {
            if (strcmp(param, "tcp") == 0)
                send_data_tcp_ipv4(ip, port);
            else if (strcmp(param, "udp") == 0)
                send_data_udp_ipv4(ip, port);
        }
        else if (strcmp(type, "ipv6") == 0)
        {
            if (strcmp(param, "tcp") == 0)
                send_data_tcp_ipv6(ip, port);
            else if (strcmp(param, "udp") == 0)
                send_data_udp_ipv6(ip, port);
        }
        else if (strcmp(type, "uds") == 0)
        {
            if (strcmp(param, "dgram") == 0)
                send_data_uds_dgram("/tmp/uds_dgram_socket");
            else if (strcmp(param, "stream") == 0)
                send_data_uds_stream("/tmp/uds_stream_socket");
        }
        else if (strcmp(type, "mmap") == 0)
        {
            send_data_mmap("/tmp/uds_mmap_socket", "try.txt", "uds_mmap");
        }
        else if (strcmp(type, "pipe") == 0)
        {
            send_data_pipe("/tmp/pipe_socket");
        }
        else
        {
            printf("Invalid client type\n");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        printf("Invalid type or param\n");
        exit(EXIT_FAILURE);
    }
    close(client_socket);
}

void stnc2_server(int port, bool quiet_mode)
{
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        perror("bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) == -1)
    {
        perror("listen");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    const int MAX_CLIENTS = 10;
    struct pollfd fds[MAX_CLIENTS + 1];
    memset(fds, 0, sizeof(fds));

    fds[0].fd = server_socket;
    fds[0].events = POLLIN;

    int nfds = 1; // Number of active file descriptors

    while (1)
    {
        int num_ready = poll(fds, nfds, -1);
        if (num_ready == -1)
        {
            perror("poll");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < nfds; i++)
        {
            if (fds[i].revents & POLLIN)
            {
                if (fds[i].fd == server_socket) // New client connection
                {
                    struct sockaddr_in client_address;
                    socklen_t client_address_len = sizeof(client_address);
                    int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);
                    if (client_socket == -1)
                    {
                        perror("accept");
                        continue;
                    }

                    if (nfds == MAX_CLIENTS + 1)
                    {
                        fprintf(stderr, "Max clients reached. Connection rejected.\n");
                        close(client_socket);
                        continue;
                    }

                    fds[nfds].fd = client_socket;
                    fds[nfds].events = POLLIN;
                    nfds++;

                    if (!quiet_mode)
                    {
                        char client_ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(client_address.sin_addr), client_ip, INET_ADDRSTRLEN);
                        printf("New connection from %s:%d\n", client_ip, ntohs(client_address.sin_port));
                    }
                }
                else // Data received from a connected client
                {
                    char type_param[256];
                    while (1)
                    {
                        ssize_t num_bytes = recv(fds[i].fd, type_param, sizeof(type_param) - 1, 0);
                        if (num_bytes <= 0)
                        {
                            if (num_bytes == 0)
                            {
                                if (!quiet_mode)
                                {
                                    printf("Socket %d hung up\n", fds[i].fd);
                                }
                            }
                            else
                            {
                                perror("recv");
                            }
                            close(fds[i].fd);

                            for (int j = i; j < nfds - 1; j++)
                            {
                                fds[j] = fds[j + 1];
                            }

                            nfds--;

                            break;
                        }

                        type_param[num_bytes] = '\0';

                        // Split type_param into type and param
                        char *type = strtok(type_param, ":");
                        char *param = strtok(NULL, ":");

                        if (type != NULL && param != NULL)
                        {
                            if (strcmp(type, "ipv4") == 0)
                            {
                                if (strcmp(param, "tcp") == 0)
                                    receive_data_tcp_ipv4(port);
                                else if (strcmp(param, "udp") == 0)
                                    receive_data_udp_ipv4(port);
                            }
                            else if (strcmp(type, "ipv6") == 0)
                            {
                                if (strcmp(param, "tcp") == 0)
                                    receive_data_tcp_ipv6(port);
                                else if (strcmp(param, "udp") == 0)
                                    receive_data_udp_ipv6(port);
                            }
                            else if (strcmp(type, "uds") == 0)
                            {
                                if (strcmp(param, "dgram") == 0)
                                    receive_data_uds_dgram("/tmp/uds_dgram_socket");
                                else if (strcmp(param, "stream") == 0)
                                    receive_data_uds_stream("/tmp/uds_stream_socket");
                            }
                            else if (strcmp(type, "mmap") == 0)
                            {
                                receive_data_mmap("/tmp/uds_mmap_socket", "try.txt", "uds_mmap");
                            }
                            else if (strcmp(type, "pipe") == 0)
                            {
                                receive_data_pipe("/tmp/pipe_socket");
                            }
                            else
                            {
                                printf("Invalid type or param\n");
                                exit(EXIT_FAILURE);
                            }
                        }
                        else
                        {
                            printf("Invalid type_param format\n");
                            exit(EXIT_FAILURE);
                        }
                    }
                }
            }
        }
    }
}

int main_stnc2(int argc, char *argv[])
{
    // Variables for storing command line arguments
    char *ip = NULL;
    int port;
    char *type = NULL;
    char *param = NULL;
    bool is_server = false;
    bool is_client = false;
    bool quiet_mode = false;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-s") == 0)
        {
            is_server = true;
            port = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-c") == 0)
        {
            is_client = true;
            ip = argv[++i];
            port = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-p") == 0)
        {
            type = argv[++i];
            param = argv[++i];
        }
        else if (strcmp(argv[i], "-q") == 0)
        {
            quiet_mode = true;
        }
    }

    // Check if we're running as a server or client
    if (is_server)
    {
        stnc2_server(port, quiet_mode);
    }
    else if (is_client)
    {
        stnc2_client(ip, port, type, param);
    }
    else
    {
        printf("Invalid usage. Please specify either server (-s) or client (-c).\n");
        return -1;
    }

    return 0;
}
