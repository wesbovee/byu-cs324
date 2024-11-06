// Replace PUT_USERID_HERE with your actual BYU CS user id, which you can find
// by running `id -u` on a CS lab machine.
#define USERID 1823704807

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

int verbose = 0;

void print_bytes(unsigned char *bytes, int byteslen);

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s server port level seed\n", argv[0]);
        return 1;
    }

    char *server = argv[1];
    int port = atoi(argv[2]);
    int level = atoi(argv[3]);
    int seed = atoi(argv[4]);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    char port_str[10];
    sprintf(port_str, "%d", port);
    int status = getaddrinfo(server, port_str, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == -1) {
        perror("socket");
        return 1;
    }

    unsigned char request[8];
    unsigned int user_id = 1823704807;
    unsigned short seed_network = htons((unsigned short)seed);
    request[0] = 0;
    request[1] = (unsigned char)level;
    memcpy(&request[2], &user_id, sizeof(user_id));
    memcpy(&request[6], &seed_network, sizeof(seed_network));

    int sent_bytes = sendto(sock, request, 8, 0, res->ai_addr, res->ai_addrlen);
    if (sent_bytes == -1) {
        perror("sendto");
        return 1;
    }

    unsigned char response[256];
    socklen_t addr_len = res->ai_addrlen;
    int received_bytes = recvfrom(sock, response, sizeof(response), 0, res->ai_addr, &addr_len);
    if (received_bytes == -1) {
        perror("recvfrom");
        return 1;
    }

    unsigned char chunk_len = response[0];
    char chunk[chunk_len + 1];
    memcpy(chunk, &response[1], chunk_len);
    chunk[chunk_len] = '\0';
    unsigned char op_code = response[chunk_len + 1];
    unsigned short op_param;
    memcpy(&op_param, &response[chunk_len + 2], sizeof(op_param));
    op_param = ntohs(op_param);
    unsigned int nonce;
    memcpy(&nonce, &response[chunk_len + 4], sizeof(nonce));
    nonce = ntohl(nonce);

    printf("Chunk Length: %d\n", chunk_len);
    printf("Chunk: %s\n", chunk);
    printf("Op Code: %d\n", op_code);
    printf("Op Param: %d\n", op_param);
    printf("Nonce: %u\n", nonce);

    unsigned int next_nonce = htonl(nonce + 1);
    unsigned char follow_up[4];
    memcpy(follow_up, &next_nonce, sizeof(next_nonce));

    print_bytes(follow_up, sizeof(follow_up));

    sent_bytes = sendto(sock, follow_up, sizeof(follow_up), 0, res->ai_addr, res->ai_addrlen);
    if (sent_bytes == -1) {
        perror("sendto");
        return 1;
    }

    close(sock);
    return 0;
}

void print_bytes(unsigned char *bytes, int byteslen) {
    int i, j, byteslen_adjusted;

    if (byteslen % 8) {
        byteslen_adjusted = ((byteslen / 8) + 1) * 8;
    } else {
        byteslen_adjusted = byteslen;
    }
    for (i = 0; i < byteslen_adjusted + 1; i++) {
        if (!(i % 8)) {
            if (i > 0) {
                for (j = i - 8; j < i; j++) {
                    if (j >= byteslen_adjusted) {
                        printf("  ");
                    } else if (j >= byteslen) {
                        printf("  ");
                    } else if (bytes[j] >= '!' && bytes[j] <= '~') {
                        printf(" %c", bytes[j]);
                    } else {
                        printf(" .");
                    }
                }
            }
            if (i < byteslen_adjusted) {
                printf("\n%02X: ", i);
            }
        } else if (!(i % 4)) {
            printf(" ");
        }
        if (i >= byteslen_adjusted) {
            continue;
        } else if (i >= byteslen) {
            printf("   ");
        } else {
            printf("%02X ", bytes[i]);
        }
    }
    printf("\n");
    fflush(stdout);
}

