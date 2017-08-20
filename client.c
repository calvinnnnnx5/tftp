#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#define PORT 6012

void timeoutHandler(int x) {
    //TODO: handle timeout
    // Check if sendAttempts = 0 (10 timeouts)
        // If so, exit
    // check last received ack block & current block
    // resend next block after last ack block
    //Reset timer    
}

int main (int argc, char const *argv[]) {
    int sendAttempts = 10;
    int lastAckBlock = -1;
    int blockNum = 0;
    int serverBlockNum = 0;

    struct sockaddr_in client_addr;
    struct sockaddr_in server_addr;

    int fd = socket(AF_INET,SOCK_DGRAM,0);

    if (fd < 0) {
        perror("fd is less than 0");
        exit(-1);
    }

    memset( (char*)&server_addr,0,sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    struct hostent hp;
    hp = gethostbyname("localhost");
    signal(SIGALRM,timeoutHandler);
    
        
    //TODO: Whatever is below this.
    //Create a string/char[] to send
    //While loop:
        //check if received anything
        //if so, process packet, send back ACK
        //send info
}
