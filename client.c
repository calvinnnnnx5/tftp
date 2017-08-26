#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#define PORT 6012

typedef int bool;
#define true 1
#define false 0

static int sendAttempts = 10;
static const int TIMEOUT_BASE = 1;
static int timeoutInterval = TIMEOUT_BASE; 
static int* sentPackets = NULL;
static char buff[516] = memset( (char*)&buff,0,sizeof(buff));

void timeoutHandler(int x) {

    if (x == SIGALRM) {
        bool notDone = true;

        //Exit/terminate after 10 consecutive tries
        if (!sendAttempts) {
            exit(-1);

        //If still unacknowledged & less than 10 consecutive tries,
        //resend packet again, double timer, & decrement sendAttempts
        } else {
            //resend packet
            

            //reset timer
            timeoutInterval = timeoutInterval * 2;
            alarm(timeoutInterval);

            //decrease sendAttempts by 1
            sendAttempts--;
        }
    }
}

void readOrWrite(char[516] buff, bool readMode) {
    char mode[] = {0};
    char filename[] = {0};
    char* buffer = NULL; 
}

int main (int argc, char const *argv[]) {

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

    struct hostent* hp;
    hp = gethostbyname("localhost");
    signal(SIGALRM,timeoutHandler);
    
        
    //TODO: Whatever is below this.
    //Create a string/char[] to send
    char data[512] = memset( (char*)&data,0,sizeof(data));

    //While loop:
        //check if received anything
        //if so, process packet, send back ACK
        //send info
        
    bool notDone = true;
    int recvlen = 0;
    while(notDone) {
        //Only do something if a packet is recieved
        //Otherwise wait for an ACK packet (or timeout)
        recvlen = recvfrom(fd,buff,516,(struct sockaddr*)&serveraddr, sizeof(serveraddr));
        if (recvlen) { 
            char opcode = buff[0]; 

            switch(opcode){
                case 1:     //RRQ
                case 2:     //WRQ
                case 3:     //DATA
                case 4:     //ACK
                default: printf("ERROR?\n");    //ERROR (5)
            }          
        }
    }
}
