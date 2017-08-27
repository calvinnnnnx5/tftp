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
static int timeoutInterval = 1; 
static int* sentPackets = NULL;
static char buff[516] = {0};
static struct sockaddr_in client_addr;
static struct sockaddr_in server_addr;

static int fd = -1;

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

bool validFileName(char fileName[512]) {
    int i = 0;
    while(fileName[i] != '\0') {
        if (fileName[i] == '/') {
            return false;
        }
        i++;
    }

    return true;
}

void readOrWrite(char buff[516], bool writeMode) {
    char mode[6] = {0};
    char fileName[512] = {0};
    char* buffPtr = &buff;
    char* strPtr = &fileName; 
    
    //Get filename from buffer
    while (*buffPtr != '\0') {
        *strPtr = *buffPtr;
        strPtr++; buffPtr++;
    }

    if( !validFileName(fileName) ) {
        //send error packet
        return;
    }
    
    buffPtr++;
    strPtr = &mode;
    int counter = 0;
    //Get mode from buffer
    while (*buffPtr != '\0') {

        //mode MUST be octet (5 char + 1 null char)
        counter++;
        if (counter > 5) {
            //send error packet
            return;
        }

        *strPtr = *buffPtr;
        strPtr++; buffPtr++;   
    }

    *strPtr = '\0';

    FILE fp;
    //create or append to file with fileName if write mode
    if (writeMode) {
        char* filePtr = &fileName;
        fp = *fopen(filePtr,"a");
    }

    //send ACK packet
    //while loop to receive whatevea
    int recvlen;
    while(true) {
        recvlen = recvfrom(fd,buff,sizeof(buff),0,(struct socketaddr*)&server_addr, sizeof(server_addr));
    }
}

int main (int argc, char const *argv[]) {

    fd = socket(AF_INET,SOCK_DGRAM,0);

    if (fd < 0) {
        perror("fd is less than 0");
        exit(-1);
    }

    memset( (char*)&buff,0,sizeof(buff));
    memset( (char*)&server_addr,0,sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    struct hostent* hp;
    hp = gethostbyname("localhost");
    signal(SIGALRM,timeoutHandler);
    
        
    //TODO: Whatever is below this.
    //Create a string/char[] to send
    char data[512];
    memset( (char*)&data,0,sizeof(data));

    //While loop:
        //check if received anything
        //if so, process packet, send back ACK
        //send info
        
    bool notDone = true;
    int recvlen = 0;
    while(notDone) {
        //Only do something if a packet is recieved
        //Otherwise wait for an ACK packet (or timeout)
        recvlen = recvfrom(fd,buff,sizeof(buff),0,(struct sockaddr*)&server_addr, sizeof(server_addr));
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
