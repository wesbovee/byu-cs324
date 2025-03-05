#define USERID 1823704807

#define BUF_SIZE 500

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "sockhelper.h"

int verbose = 0;

void print_bytes(unsigned char *bytes, int byteslen);

int main(int argc, char *argv[]) {
	char * server = argv[1];
	int port = atoi(argv[2]);
	int level = atoi(argv[3]);
	int seed = atoi(argv[4]);
	unsigned char message[10];
	char port_string[6];
	snprintf(port_string,sizeof(port_string), "%d", port);
	int USERID_ = htonl(USERID);
	message[0] = 0;
	message[1] = (unsigned char)level;

	// Copy USERID_ (ensure network byte order)
	memcpy(&message[2], &USERID_, sizeof(uint32_t));

	// Copy seed (ensure network byte order)
	uint16_t seed_net = htons(seed);
	memcpy(&message[6], &seed_net, sizeof(uint16_t));
	int addr_fam = AF_INET;
	int sock_type = SOCK_DGRAM;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = addr_fam;
	hints.ai_socktype = sock_type;
	struct addrinfo *result;
	int s;
	s = getaddrinfo(server, port_string, &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}
	struct sockaddr_storage remote_addr_ss;
	struct sockaddr *remote_addr = (struct sockaddr *)&remote_addr_ss;
	struct addrinfo * rp;
	char remote_ip[INET6_ADDRSTRLEN];
	unsigned short remote_port;
	int sfd;
	socklen_t remote_addr_len;
	for (rp=result; rp!=NULL; rp = rp->ai_next) {

		sfd = socket(rp->ai_family, rp->ai_socktype, 0);
		if (sfd < 0) {
			// error creating the socket
			continue;
		}
		memcpy(remote_addr, rp->ai_addr, rp->ai_addrlen);
		remote_addr_len = rp->ai_addrlen;
		break;

	}
	if (rp == NULL) {
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}

	char buf[BUF_SIZE];
	freeaddrinfo(result);
	parse_sockaddr(remote_addr, remote_ip, &remote_port);
	struct sockaddr_storage local_addr_ss;
	struct sockaddr *local_addr = (struct sockaddr *)&local_addr_ss;

	char local_ip[INET6_ADDRSTRLEN];
	unsigned short local_port;
	socklen_t addr_len = remote_addr_len;
	sendto(sfd, message, 8, 0, remote_addr, addr_len);
	s = getsockname(sfd, local_addr, &addr_len);
	parse_sockaddr(local_addr, local_ip, &local_port);
	int done = 1;
	char treasure[1024];
	int treasure_len = 0;
	while (done){
		recvfrom(sfd, buf, BUF_SIZE, 0,remote_addr, &addr_len);
		unsigned char chunk_length = buf[0];
		if (chunk_length == 0){
			done = 0;
			break;
		}
		if (chunk_length >0 && chunk_length < 128){
		memcpy(&treasure[treasure_len], &buf[1], chunk_length);
		treasure_len+=chunk_length;
		treasure[treasure_len] = '\0';
		char chunk[chunk_length+1];
		memcpy(chunk, &buf[1], chunk_length);
		chunk[chunk_length] = '\0';
		unsigned char op_code;
		memcpy(&op_code, &buf[chunk_length+1], sizeof(op_code));
		unsigned short op_param;
		memcpy(&op_param, &buf[chunk_length+2], sizeof(op_param));
		unsigned int Nonce;
		memcpy(&Nonce, &buf[chunk_length+4], sizeof(int));
		Nonce = ntohl(Nonce);
		Nonce++;
		unsigned int new_nonce = htonl(Nonce);
		unsigned char new_message[4];
		memcpy(new_message, &new_nonce, sizeof(int));
		if (op_code == 1){
			remote_port = htons(op_param);
			populate_sockaddr(remote_addr, addr_fam, remote_ip, remote_port);
		}
		if (op_code == 2){
			local_port = htons(op_param);
			close(sfd);
			sfd = socket(addr_fam, sock_type, 0);
			populate_sockaddr(local_addr, addr_fam, NULL, local_port);
			bind(sfd, local_addr, sizeof(local_addr_ss));
		}
		if (op_code == 3){
			op_param = ntohs(op_param);
			unsigned short temp_remote_port;
			unsigned int Nounce =0;
			for (int i=0; i<op_param; i++){
				struct sockaddr_storage temp_remot_addr_ss;
				struct sockaddr *temp_remote_addr = (struct sockaddr *)&temp_remot_addr_ss;
				recvfrom(sfd, buf, BUF_SIZE, 0, temp_remote_addr, &addr_len);
				parse_sockaddr(temp_remote_addr, remote_ip, &temp_remote_port);
				Nounce+=temp_remote_port;
			}
			Nounce++;
			Nounce = htonl(Nounce);
			memcpy(new_message, &Nounce, sizeof(int));
		}
		if (op_code == 4){
			remote_port = htons(op_param);
			sprintf(port_string, "%d", remote_port);
			if (hints.ai_family == AF_INET){
				hints.ai_family = AF_INET6;
				s = getaddrinfo(server, port_string, &hints, &result);
				addr_fam = AF_INET6;
			}
			else{
				hints.ai_family = AF_INET;
				s =getaddrinfo(server, port_string, &hints, &result);
				addr_fam = AF_INET;
				}
			close(sfd);
			for (rp=result; rp!=NULL; rp = rp->ai_next) {
				sfd = socket(rp->ai_family, rp->ai_socktype, 0);
				if (sfd < 0) {
					// error creating the socket
					continue;
				}
				memcpy(remote_addr, rp->ai_addr, rp->ai_addrlen);
				addr_len = rp->ai_addrlen;
				break;
				}
				if (rp == NULL) {
					fprintf(stderr, "Could not connect\n");
					exit(EXIT_FAILURE);
				}
				freeaddrinfo(result);
				parse_sockaddr(remote_addr, remote_ip, &remote_port);

				// bind(sfd, local_addr, sizeof(local_addr_ss));

		}
		sendto(sfd, new_message, 4, 0, remote_addr, addr_len);
		}
		else{
			printf("Invalid chunk length %u\n", chunk_length);
			done = 0;
		}

	}
	printf("%s\n", treasure);
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
