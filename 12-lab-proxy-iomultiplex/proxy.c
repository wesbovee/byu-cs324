
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "sockhelper.h"

/* Recommended max object size */
#define MAX_OBJECT_SIZE 16384

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";

#define READ_REQUEST 1
#define SEND_REQUEST 2
#define READ_RESPONSE 3
#define SEND_RESPONSE 4
#define MAXEVENTS 10

struct request_info {
	int state;
	int client_fd;
	int server_fd;
	int bytes_read;
	int bytes_sent;
	int total_bytes;
	int total_bytes_response;
	int total_bytes_sent_client;
	char buffer[MAX_OBJECT_SIZE];
	char buffer_response[MAX_OBJECT_SIZE];
	char method[16];
	char hostname[64];
	char port[8];
	char path[64];
};

int complete_request_received(char *);
void parse_request(char *, char *, char *, char *, char *);
void test_parser();
void print_bytes(unsigned char *, int);
void getServerRequest(char *, char *, char *, char *, char *);
void handle_new_clients(int, int);
int handle_client(int, struct request_info *);

int open_sfd(int port) {
	int sfd;
	if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Error creating socket");
		return -1;
	}
	int optval = 1;
	setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	if (fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
		fprintf(stderr, "error setting socket option\n");
		exit(1);
	}
	struct sockaddr_in server;
	memset(&server, 0, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	bind(sfd, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
	listen(sfd, 100);
	return sfd;
}
void handle_new_clients(int epoll_fd, int sfd) {
	while (1) {
	// Declare structures for remote address and port.
	// See notes above for local_addr_ss and local_addr_ss.
	struct sockaddr_storage remote_addr_ss;
	struct sockaddr *remote_addr = (struct sockaddr *)&remote_addr_ss;
	char remote_ip[INET6_ADDRSTRLEN];
	unsigned short remote_port;

	socklen_t addr_len = sizeof(struct sockaddr_storage);
	int connfd = accept(sfd, remote_addr, &addr_len);
	if (connfd < 0) {
		if (errno == EWOULDBLOCK ||
				errno == EAGAIN) {
			// no more clients ready to accept
			break;
		} else {
			perror("accept");
			exit(EXIT_FAILURE);
		}
	}

	if (connfd < 0) {
		if (errno == EWOULDBLOCK ||
				errno == EAGAIN) {
			// no more clients ready to accept
			break;
		} else {
			perror("accept");
			exit(EXIT_FAILURE);
		}
	}
	parse_sockaddr(remote_addr, remote_ip, &remote_port);
					printf("Connection from %s:%d\n",
							remote_ip, remote_port);

					
					// set client file descriptor nonblocking
					if (fcntl(connfd, F_SETFL, fcntl(connfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
						fprintf(stderr, "error setting socket option\n");
						exit(1);
					}

	struct request_info *new_client = (struct request_info *)malloc(sizeof(struct request_info));
	new_client->client_fd = connfd;
	new_client->state = READ_REQUEST;
	struct epoll_event event;
	event.data.ptr = new_client;
	event.events = EPOLLIN | EPOLLET;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, connfd, &event) < 0) {
		perror("epoll_ctl");
		exit(EXIT_FAILURE);
		}
	//Have your HTTP proxy print the newly created file descriptor associated with any new clients. You can remove this later, but it will be good for you to see now that they are being created.
	printf("New client file descriptor: %d\n", connfd);
}

}
int handle_client(int epoll_fd,struct request_info *active_client) {
	//print out both the file descriptor associated with the client-to-proxy socket and the current state.
	printf("Client file descriptor: %d\n", active_client->client_fd);
	printf("Current state: %d\n", active_client->state);
	if (active_client->state == READ_REQUEST){
		while (1){
			int bytes_read = read(active_client->client_fd, active_client->buffer + active_client->total_bytes, MAX_OBJECT_SIZE);
			if (complete_request_received(active_client->buffer)){
				active_client->total_bytes += bytes_read;
				active_client->buffer[active_client->total_bytes] = '\0';
				parse_request(active_client->buffer, active_client->method, active_client->hostname, active_client->port, active_client->path);
				printf("METHOD: %s\n", active_client->method);
				printf("HOSTNAME: %s\n", active_client->hostname);
				printf("PORT: %s\n", active_client->port);
				printf("PATH: %s\n", active_client->path);
				// Format the HTTP request properly
				memset(active_client->buffer, 0, MAX_OBJECT_SIZE); // Clear the buffer
				active_client->total_bytes = 0;
				active_client->bytes_sent = 0;
				
				// Build the request in the buffer
				int len = snprintf(active_client->buffer, MAX_OBJECT_SIZE,
					"%s %s HTTP/1.0\r\n"
					"Host: %s%s%s\r\n"
					"%s\r\n"
					"Connection: close\r\n"
					"Proxy-Connection: close\r\n"
					"\r\n",
					active_client->method,
					active_client->path,
					active_client->hostname,
					(strcmp(active_client->port, "80") != 0) ? ":" : "",
					(strcmp(active_client->port, "80") != 0) ? active_client->port : "",
					user_agent_hdr);
				
				if (len < 0 || len >= MAX_OBJECT_SIZE) {
					fprintf(stderr, "Error formatting HTTP request or request too large\n");
					return -1;
				}
				
				active_client->total_bytes = len;
				printf("Formatted HTTP request (%d bytes):\n%s\n", len, active_client->buffer);
				struct addrinfo temp, *res,*p;
				memset(&temp, 0, sizeof(temp));
				temp.ai_family = AF_INET;
				temp.ai_socktype = SOCK_STREAM;
				printf("Connecting to server\n");
				int status = getaddrinfo(active_client->hostname, active_client->port, &temp, &res);
				printf("Got address info\n");
				if (status != 0) {
					printf("getaddrinfo error\n");
					fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
					return -1;
				}
				int sockfd = -1; // Initialize to avoid uninitialized warning
				for (p = res; p != NULL; p = p->ai_next) {
					sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
					if (sockfd < 0) {
						// close(sockfd);
						continue;
					}
					if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
						perror("connect");
						close(sockfd);
						continue;
					}
					break;
				}
				printf("Connected to server\n");
				if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
							fprintf(stderr, "error setting socket option\n");
							exit(1);
							}
				// Remove client fd from epoll
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, active_client->client_fd, NULL);
				
				// Add server fd to epoll with active_client as data
				struct epoll_event event;
				event.data.ptr = active_client;
				event.events = EPOLLOUT | EPOLLET;
				epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &event);
				printf("Switched to sending request\n");
				active_client->state = SEND_REQUEST;
				active_client->server_fd = sockfd;
				break;
			}
			if (bytes_read < 0) {
				if (errno == EWOULDBLOCK || errno == EAGAIN) {
					break;
				} else {
					perror("read");
					close(active_client->client_fd);
					free(active_client);
					break;
				}
			}
			active_client->total_bytes += bytes_read;
			
		}
	}
	if (active_client->state == SEND_REQUEST){
		while (1){
			int bytes_sent = write(active_client->server_fd, 
				active_client->buffer + active_client->bytes_sent, 
				active_client->total_bytes - active_client->bytes_sent);
				
			printf("Bytes sent to server: %d\n", bytes_sent);
			
			if (bytes_sent < 0) {
				if (errno == EWOULDBLOCK || errno == EAGAIN) {
					// Would block, try again later
					return 0;
				} else {
					perror("write to server");
					close(active_client->server_fd);
					close(active_client->client_fd);
					free(active_client);
					return -1;
				}
			}
			
			active_client->bytes_sent += bytes_sent;
			
			if (active_client->bytes_sent == active_client->total_bytes) {
				// All request data sent to server, now wait for response
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, active_client->server_fd, NULL);
				
				struct epoll_event event;
				event.data.ptr = active_client;
				event.events = EPOLLIN | EPOLLET;
				epoll_ctl(epoll_fd, EPOLL_CTL_ADD, active_client->server_fd, &event);
				
				printf("Request fully sent to server fd: %d\n", active_client->server_fd);
				active_client->state = READ_RESPONSE;
				active_client->total_bytes_response = 0;
				printf("Switched to reading response\n");
				break;
			}
		}
	}
	if(active_client->state == READ_RESPONSE){
		printf("Reading response\n");
		fflush(stdout);
		
		while (1) {
			int bytes_read = recv(active_client->server_fd, 
				active_client->buffer_response + active_client->total_bytes_response, 
				MAX_OBJECT_SIZE - active_client->total_bytes_response, 0);
				
			if (bytes_read == 0) {
				// End of response
				printf("End of response, total bytes: %d\n", active_client->total_bytes_response);
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, active_client->server_fd, NULL);
				close(active_client->server_fd);
				
				// Set up for sending response to client
				struct epoll_event event;
				event.data.ptr = active_client;
				event.events = EPOLLOUT | EPOLLET;
				epoll_ctl(epoll_fd, EPOLL_CTL_ADD, active_client->client_fd, &event);
				active_client->state = SEND_RESPONSE;
				active_client->total_bytes_sent_client = 0;
				break;
			}
			
			if (bytes_read < 0) {
				if (errno == EWOULDBLOCK || errno == EAGAIN) {
					// No more data available right now, but connection still open
					break;
				} else {
					perror("read from server");
					close(active_client->server_fd);
					close(active_client->client_fd);
					free(active_client);
					return -1;
				}
			}
			
			active_client->total_bytes_response += bytes_read;
			printf("Total bytes read from server: %d\n", active_client->total_bytes_response);
			
			// Check if buffer is full
			if (active_client->total_bytes_response >= MAX_OBJECT_SIZE) {
				printf("Response buffer full\n");
				break;
			}
		}
	}
	if(active_client->state == SEND_RESPONSE){
		while (active_client->total_bytes_sent_client < active_client->total_bytes_response) {
			int bytes_sent = send(active_client->client_fd, 
				active_client->buffer_response + active_client->total_bytes_sent_client, 
				active_client->total_bytes_response - active_client->total_bytes_sent_client, 0);
				
			if (bytes_sent < 0) {
				if (errno == EWOULDBLOCK || errno == EAGAIN) {
					// Would block, try again later
					return 0;
				} else {
					perror("write to client");
					close(active_client->client_fd);
					free(active_client);
					return -1;
				}
			}
			
			active_client->total_bytes_sent_client += bytes_sent;
			printf("Total bytes sent to client: %d of %d\n", 
				active_client->total_bytes_sent_client, active_client->total_bytes_response);
		}
		
		// All data sent, clean up
		printf("Response fully sent, closing connection\n");
		epoll_ctl(epoll_fd, EPOLL_CTL_DEL, active_client->client_fd, NULL);
		close(active_client->client_fd);
		free(active_client);
	}
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	int efd;
	if ((efd = epoll_create1(0)) < 0) {
		perror("Error with epoll_create1");
		exit(EXIT_FAILURE);
	}
	
	int sfd = open_sfd(atoi(argv[1]));
	if (sfd < 0) {
		fprintf(stderr, "Failed to open socket on port %s\n", argv[1]);
		exit(EXIT_FAILURE);
	}
	
	struct epoll_event event;
	event.data.fd = sfd;
	event.events = EPOLLIN | EPOLLET;
	if (epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event) < 0) {
		perror("epoll_ctl");
		exit(EXIT_FAILURE);
	}
	
	struct epoll_event *events = calloc(MAXEVENTS, sizeof(struct epoll_event));
	if (!events) {
		perror("Failed to allocate events array");
		exit(EXIT_FAILURE);
	}
	
	printf("Proxy server running on port %s\n", argv[1]);
	
	while (1) {
		int n = epoll_wait(efd, events, MAXEVENTS, -1); // Wait indefinitely
		if (n < 0) {
			if (errno == EINTR) {
				continue; // Interrupted by signal, try again
			}
			perror("epoll_wait");
			exit(EXIT_FAILURE);
		}
		
		for (int i = 0; i < n; i++) {
			if (events[i].data.fd == sfd) {
				// New connection
				handle_new_clients(efd, sfd);
			} else {
				// Existing connection
				struct request_info *active_client = (struct request_info *)(events[i].data.ptr);
				if (active_client != NULL) {
					handle_client(efd, active_client);
				} else {
					fprintf(stderr, "Warning: NULL client pointer in event\n");
				}
			}
		}
	}
	
	free(events);
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
