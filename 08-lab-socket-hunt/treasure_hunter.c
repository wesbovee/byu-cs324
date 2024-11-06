#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>

#define USERID 123456789

void print_bytes(unsigned char *msg, int len) {
    for (int i = 0; i < len; i++) {
        printf("%02x ", msg[i]);
    }
    printf("\n");
}

void parse_args(int argc, char *argv[], char *server, int *port, int *level, int *seed) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s server port level seed\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    strcpy(server, argv[1]);
    *port = atoi(argv[2]);
    *level = atoi(argv[3]);
    *seed = atoi(argv[4]);
    printf("Server: %s\nPort: %d\nLevel: %d\nSeed: %d\n", server, *port, *level, *seed);
}

void create_initial_message(unsigned char *msg, int level, int user_id, int seed) {
    msg[0] = 0;
    msg[1] = (unsigned char)level;

    uint32_t user_id_network = htonl(user_id); // Convert user ID to network byte order (big-endian)
    memcpy(&msg[2], &user_id_network, sizeof(user_id_network));

    uint16_t seed_network = htons(seed); // Convert seed to network byte order (big-endian)
    memcpy(&msg[6], &seed_network, sizeof(seed_network));
}

int setup_socket(struct sockaddr_in *server_addr, const char *server, int port) {
    struct addrinfo hints, *res;
    int sockfd;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    char port_str[10];
    sprintf(port_str, "%d", port);

    if (getaddrinfo(server, port_str, &hints, &res) != 0) {
        perror("getaddrinfo failed");
        exit(EXIT_FAILURE);
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memcpy(server_addr, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    return sockfd;
}

int main(int argc, char *argv[]) {
    char server[256];
    int port, level, seed;
    parse_args(argc, argv, server, &port, &level, &seed);

    unsigned char msg[8];
    create_initial_message(msg, level, USERID, seed);

    printf("Initial request message:\n");
    print_bytes(msg, 8);

    struct sockaddr_in server_addr;
    int sockfd = setup_socket(&server_addr, server, port);

    if (sendto(sockfd, msg, 8, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("sendto failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    unsigned char response[256];
    socklen_t addr_len = sizeof(server_addr);
    ssize_t num_bytes = recvfrom(sockfd, response, sizeof(response), 0, (struct sockaddr *)&server_addr, &addr_len);
    if (num_bytes == -1) {
        perror("recvfrom failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Response message:\n");
    print_bytes(response, num_bytes);

    close(sockfd);

    return 0;
}
