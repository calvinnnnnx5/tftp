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

// Make sure the filename/string is valid as specified by project writeup
bool validFileName(const char fileName[512], bool writeMode) {
    int i = 0;

    // Make sure filename includes NO paths
    // All files read & written must be in same directory
    while(fileName[i] != '\0') {
        if (fileName[i] == '/') {
            return false;
        }
        i++;
    }

    const char* fn = &fileName;

    // Check file existence & (if it exists) file permissions
    if (writeMode) {
        int res = access(fn,W_OK);
        return (res == 0);
    } else {
        int res = access(fn,R_OK);
        return (res == 0);
    }
}

void sendACK(char buff[516], int blockNumber) {

    // Get the upper half of blockNumber (shift right by 8);
    char upper = blockNumber >> 8;

    // Let blockNumber be XXXX XXXX XXXX XXXX
    // Then blockNumber ^ 65280 = XXXX XXXX XXXX XXXX ^ 1111 1111 0000 0000
    // i.e. Get the lower half of blockNumber
    char lower = blockNumber ^ 65280;

    char message[4];
    message[0] = 0;
    message[1] = 4;
    message[2] = upper;
    message[3] = lower;

    int fx = sendto(fd,message,strlen(message),0,(struct sockaddr*)&server_addr,sizeof(server_addr));
}

// Send out an error packet given an error code and an error message
void sendERR(int code, char* str) {
    char* op = "050";
    size_t size = 0;
    int index = 0;

    // Set up size of message string
    // Add 2 at the end for the code and null char
    size += strlen(str) + strlen(op) + 2;

    char* message = NULL;
    char* temp = NULL;

    // Allocate space for the message
    // Keep trying up to 10 times or until memory is allocated
    int count = 0;
    temp = realloc(message, size);
    while (!temp) {
        if (count == 10) {
            break;
        }

        count++;
        temp = realloc(message,size);
    }
    message = temp;

    // Start building string starting with op code,
    // then error code, then error message
    while (*op) {
        message[index++] = *op++;
    }    

    message[index++] = code;

    while (*str) {
        message[index++] = *str++;
    }

    message[index++] = '\0';
    
    // Send message to server
    sendto(fd,message,strlen(message),0,(struct sockaddr*)&server_addr,sizeof(server_addr))

    // Free memory for message
    free(message);
}

void waitForACK() {
    int recvlen;
    bool notDone = true;
    char buff[516];
    memset( (char*)&buff,0,sizeof(buff));
    
    while (notDone) {
        recvlen = recvfrom(fd,buff,sizeof(buff),0,(struct socketaddr*)&server_addr, sizeof(server_addr));
        
        if (recvlen > 0) {
            if (buff[1] != 4) {
                sendERR(0,"WAITING FOR ACK");
            } else {
                notDone = true;
            }
        }
    }
    return;  
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
        sendERR(2,"");
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
    } else {
        char* filePtr = &fileName;
        fp = *fopen(filePtr,"r");
    }

    //send ACK packet
    sendACK(buff,0);

    //while loop to receive/transmit whatevea
    int recvlen;
    bool notDone = true;
 
    // If in write mode, write all valid packets from server
    while(notDone && writeMode) {
        recvlen = recvfrom(fd,buff,sizeof(buff),0,(struct socketaddr*)&server_addr, sizeof(server_addr));
        if (recvlen > 0) {

            if (recvlen < 516) {
                notDone = true;
            }

            int opcode = buff[0];
            opcode << 8;
            opcode = opcode + buff[1];

            if (opcode != 3) {
                sendERR(0,"Opcode must be 3 for data");
            }

            // Check block number
            // if valid, write to file
        
        }
    }

    // If in read mode, read from file & send packets to server
    while (notDone && !writeMode) {
        // Read in 512 bytes from file, or less if there is less
        // convert to message
        // Add headers to message
        // send to server
        // wait for ACK before continuing
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
            int opcode = buff[0];
            opcode << 8;
            opcode = opcode + buff[1];

            switch(opcode){
                case 1: readOrWrite(buff,false); break;     //RRQ
                case 2: readOrWrite(buff,true);  break;     //WRQ
                case 3:     //DATA
                case 4:     //ACK
                default: printf("ERROR?\n");    //ERROR (5)
            }          
        }
    }
}
