#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#define PORT 61012

void close_socket(int socket) {
    close(socket);
}

int main(int argc, char const *argv[]) {
    int sendAttempts = 10; //Attempts to determine still connected
    int fd; //Socket's file descriptor
    int client_fd;
    int msgcnt = 0; //Number of messages received
    char buf[512];
    int byte_count;

    struct sockaddr_in server_addr; //Server socket
    struct sockaddr_in client_addr; //Client socket
    socklen_t client_len = sizeof(client_addr);
    struct hostent *hp;

    hp = gethostbyname("localhost");

    /* Create UDP socket
       Returns a file descriptor, should be small int */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd < 0) {
	perror("Failed to create socket\n");
	exit(-1);
    }

    // Bind the socket to port 61012
    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
	perror("Failed to bind socket");
	exit(-1);
    }

    //Wait for client to send read or write request
    for(;;) {
	printf("Waiting for port 61012\n");
	byte_count = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&client_addr, &client_len);
    }
}
