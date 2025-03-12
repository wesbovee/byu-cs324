#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#define SO_REUSEPORT 0x0200

#define THREAD_POOL_SIZE 8
#define QUEUE_SIZE 5
#define BUFFER_SIZE 1024 // Adjust based on expected HTTP request size.
#define BACKLOG 10       // Maximum number of queued connections.

/* Recommended max object size */
#define MAX_OBJECT_SIZE 102400

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";
int open_sfd(int);
int complete_request_received(char *);
void parse_request(char *, char *, char *, char *, char *);
void test_parser();
void print_bytes(unsigned char *, int);
int queue_fd[QUEUE_SIZE];
int this,that =0;
void *thread_function(void *arg);

pthread_mutex_t queue;
sem_t avaliable_slots;
sem_t used_slots;

int main(int argc, char *argv[])
{
	printf("%s\n", user_agent_hdr);
	int port = atoi(argv[1]);
	int sfd = open_sfd(port);


	pthread_mutex_init(&queue, NULL);
	sem_init(&avaliable_slots, 0, QUEUE_SIZE);
	sem_init(&used_slots, 0, 0);

	pthread_t threads[THREAD_POOL_SIZE];
	for (int i = 0; i < THREAD_POOL_SIZE; i++) {
		pthread_create(&threads[i], NULL, thread_function, &sfd);
	}
	while (1){
		struct sockaddr_in client;
		socklen_t client_len = sizeof(struct sockaddr_in);
		int client_fd = accept(sfd, (struct sockaddr *)&client, &client_len);
		sem_wait(&avaliable_slots);
		pthread_mutex_lock(&queue);
		queue_fd[this] = client_fd;
		this = (this+1)%QUEUE_SIZE;
		pthread_mutex_unlock(&queue);
		sem_post(&used_slots);

		if (client_fd < 0) {
			perror("accept");
			return -1;
		}
		// handle_client(client_fd);
	}
	close(sfd);
	sem_destroy(&avaliable_slots);
	sem_destroy(&used_slots);
	pthread_mutex_destroy(&queue);
	return 0;
	
}
void *thread_function(void *arg) {
    while (1) {
		sem_wait(&used_slots);
		pthread_mutex_lock(&queue);
		int client_fd = queue_fd[that];
		that = (that+1)%QUEUE_SIZE;
		pthread_mutex_unlock(&queue);
		sem_post(&avaliable_slots);
		handle_client(client_fd);
	}
	
	
}

int open_sfd(int port){
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd < 0) {
		perror("socket");
		return -1;
	}
	int optval = 1;
	setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	struct sockaddr_in server;
	memset(&server, 0, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	bind(sfd, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
	listen(sfd, BACKLOG);
	return sfd;
	
	
}
int handle_client(int fd){
	char buffer[BUFFER_SIZE];
	ssize_t total_bytes = 0;
	while (1) {
		int bytes_read = recv(fd, buffer+total_bytes, BUFFER_SIZE, 0);
		if (bytes_read < 0) {
			perror("recv");
			return -1;
		} else if (bytes_read == 0) {
			break;
		}
		total_bytes += bytes_read;
		if (complete_request_received(buffer)) {
			buffer[total_bytes] = '\0';
			break;
		}
	}
	print_bytes(buffer, total_bytes);
	char method[16], hostname[64], port[8], path[64];
	parse_request(buffer, method, hostname, port, path);
	printf("METHOD: %s\n", method);
	printf("HOSTNAME: %s\n", hostname);
	printf("PORT: %s\n", port);
	printf("PATH: %s\n", path);
	char first_line [256];
	ssize_t added_bytes =0;
	strncpy(first_line+added_bytes,method, strlen(method));
	added_bytes += strlen(method);
	first_line[added_bytes] =' ';
	added_bytes += 1;
	strncpy(first_line+added_bytes, path, strlen(path));
	added_bytes += strlen(path);
	first_line[added_bytes] =' ';
	added_bytes += 1;
	strncpy(first_line+added_bytes, "HTTP/1.0\r\n", 10);
	added_bytes += 10;
	strncpy(first_line+added_bytes, "Host: ", 6);
	added_bytes += 6;
	strncpy(first_line+added_bytes, hostname, strlen(hostname));
	added_bytes += strlen(hostname);
	if (strcmp(port, "80") != 0) {
		strncpy(first_line+added_bytes, ":", 1);
		added_bytes += 1;
		strncpy(first_line+added_bytes, port, strlen(port));
		added_bytes += strlen(port);
	}
	strncpy(first_line+added_bytes, "\r\n", 2);
	added_bytes += 2;
	strncpy(first_line+added_bytes, user_agent_hdr, strlen(user_agent_hdr));
	added_bytes += strlen(user_agent_hdr);
	strncpy(first_line+added_bytes, "\r\n", 2);
	added_bytes += 2;
	strncpy(first_line+added_bytes, "Connection: close", 17);
	added_bytes += 17;
	strncpy(first_line+added_bytes, "\r\n", 2);
	added_bytes += 2;
	strncpy(first_line+added_bytes, "Proxy-Connection: close", 23);
	added_bytes += 23;
	strncpy(first_line+added_bytes, "\r\n", 2);
	added_bytes += 2;
	strncpy(first_line+added_bytes, "\r\n", 2);
	added_bytes += 2;
	print_bytes(first_line, added_bytes);
	struct addrinfo temp, *res,*p;
	memset(&temp, 0, sizeof(temp));
	temp.ai_family = AF_INET;
	temp.ai_socktype = SOCK_STREAM;
	printf("Connecting to server\n");
	int status = getaddrinfo(hostname, port, &temp, &res);
	printf("Got address info\n");
	if (status != 0) {
		printf("getaddrinfo error\n");
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		return -1;
	}
	int sockfd;
	printf("Connecting to server\n");
	for (p = res; p != NULL; p = p->ai_next) {
        // Create a socket
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            perror("socket");
            continue;
        }
		printf("Socket created\n");

        // Attempt to connect
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("connect");
            close(sockfd);
            continue;
        }
		printf("Connected to server\n");
        // Successfully connected
        break;
    }
	if (p == NULL) {
    fprintf(stderr, "Failed to connect to %s:%s\n", hostname, port);
    freeaddrinfo(res);
    close(fd); // Close the client socket
    return -1;
}

	freeaddrinfo(res);
	printf("Connected to server\n");
	status = send(sockfd, first_line, added_bytes, 0);
	if (status < 0) {
		printf("send error\n");
		perror("send");
		close(sockfd);
		return -1;
	}
	printf("Sent request to server\n");
	char serverbuffer [16384];
	total_bytes=0;
	ssize_t bytes_read;
	while ((bytes_read = recv(sockfd, serverbuffer + total_bytes, BUFFER_SIZE, 0)) > 0) {
    int bytes_sent = 0;
    while (bytes_sent < bytes_read) {
        status = send(fd, serverbuffer + total_bytes + bytes_sent, bytes_read - bytes_sent, 0);
        if (status < 0) {
            perror("send");
            return -1;
        }
        bytes_sent += status;
    }
    total_bytes += bytes_read;
}
if (bytes_read < 0) {
    perror("recv");
    return -1;
}
	serverbuffer[total_bytes] = '\0';
	print_bytes(serverbuffer, total_bytes);
	close(sockfd);
	close(fd);

	return 0;
}

int complete_request_received(char *request) {
	if (strstr(request, "\r\n\r\n")!= NULL){
		return 1;
	} else {
		return 0;
	}
	
}

void parse_request(char *request, char *method,
		char *hostname, char *port, char *path) {
			char *beginning_of_thing = request;
			char *end_of_thing = strstr(beginning_of_thing, " ");
			strncpy(method, beginning_of_thing, end_of_thing - beginning_of_thing);
			method[end_of_thing - beginning_of_thing] = '\0';
			beginning_of_thing = end_of_thing + 1;
			end_of_thing = strstr(beginning_of_thing, " ");
			char url [256];
			strncpy(url, beginning_of_thing, end_of_thing - beginning_of_thing);
			url[end_of_thing - beginning_of_thing] = '\0';		
			beginning_of_thing =url;
			beginning_of_thing +=7;
			char *port_start = strchr(beginning_of_thing, ':');
    		char *path_start = strchr(beginning_of_thing, '/');
			if (port_start && (path_start == NULL || port_start < path_start)) {
				end_of_thing = strstr(beginning_of_thing, ":");
				strncpy(hostname, beginning_of_thing, end_of_thing - beginning_of_thing);
				hostname[end_of_thing - beginning_of_thing] = '\0';
				beginning_of_thing = end_of_thing + 1;
				end_of_thing = strstr(beginning_of_thing, "/");
				strncpy(port, beginning_of_thing, end_of_thing - beginning_of_thing);
				port[end_of_thing - beginning_of_thing] = '\0';
				if (path_start) {
					beginning_of_thing = path_start;
					strcpy(path, beginning_of_thing);

				} else {
					strcpy(path, "/");
				}
			} else {
				if (path_start) {
					end_of_thing = strstr(beginning_of_thing, "/");
					strncpy(hostname, beginning_of_thing, end_of_thing - beginning_of_thing);
					hostname[end_of_thing - beginning_of_thing] = '\0';
					beginning_of_thing = end_of_thing;
					strcpy(path, beginning_of_thing);
				} else {
					end_of_thing = strstr(beginning_of_thing, "/");
					strncpy(hostname, beginning_of_thing, end_of_thing - beginning_of_thing);
					hostname[end_of_thing - beginning_of_thing] = '\0';
					strcpy(path, "/");
				}
				strcpy(port, "80");
			};

}

void test_parser() {
	int i;
	char method[16], hostname[64], port[8], path[64];

       	char *reqs[] = {
		"GET http://www.example.com/index.html HTTP/1.0\r\n"
		"Host: www.example.com\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://www.example.com:8080/index.html?foo=1&bar=2 HTTP/1.0\r\n"
		"Host: www.example.com:8080\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://localhost:1234/home.html HTTP/1.0\r\n"
		"Host: localhost:1234\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://www.example.com:8080/index.html HTTP/1.0\r\n",

		NULL
	};
	
	for (i = 0; reqs[i] != NULL; i++) {
		printf("Testing %s\n", reqs[i]);
		if (complete_request_received(reqs[i])) {
			printf("REQUEST COMPLETE\n");
			parse_request(reqs[i], method, hostname, port, path);
			printf("METHOD: %s\n", method);
			printf("HOSTNAME: %s\n", hostname);
			printf("PORT: %s\n", port);
			printf("PATH: %s\n", path);
		} else {
			printf("REQUEST INCOMPLETE\n");
		}
	}
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
