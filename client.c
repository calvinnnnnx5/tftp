#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#define PORT 6112

typedef int bool;
#define true 1
#define false 0

static int sendAttempts = 10;
static const int TIMEOUT_BASE = 1;
static int timeoutInterval = 1; 
static int expectedBlock;
static char[512] lastData;
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
            sendData(lastData, expectedBlock);

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

    if (writeMode) {
        int res = acces(fileName,R_OK);
        return (res == 0);
    
    } else  {
        int res = access(fileName, F_OK);
        return (res != 0);
    }

    return true;
}

int sendData(const char data[], int blockNumber) {
    char* message = NULL;
    size_t size = 0;
    int index = 0;

    // Add 4 for 2 bytes dedicated to block number
    // and 2 more bytes for the opcode
    // Allocate the total number of bytes needed to message
    size += strlen(data) + 4;
    message = realloc(message, size);

    // Add opcode first
    message[index++] = 0;
    message[index++] = 3;

    // Insert bits 8-15 of block number into first block number byte
    char blockbuff = blockNumber >> 8;
    message[index++] = blockbuff;

    // Insert bits 0-7 of block number into second block number byte
    blockbuff = blockNumber ^ 65288;
    message[index++] = blockbuff;

    // Insert actual data (up to 512 bytes),
    // checked before calling this function
    char* ptr = data;
    while(*ptr) {
        message[index++] = *ptr++;
    }

    // Send message
    return sendto(fd,message,strlen(message),0,(struct sockaddr*)&server_addr,sizeof(server_addr));     
}

void sendACK(char buff[516], int blockNumber) {

    // Get the upper half of blockNumber (shift right by 8);
    char upper = blockNumber >> 8;

    // Let blockNumber be XXXX XXXX XXXX XXXX
    // Then blockNumber ^ 65280 = XXXX XXXX XXXX XXXX ^ 1111 1111 0000 0000
    // i.e. Get the lower half of blockNumber
    char lower = blockNumber ^ 65280;

    char message[4] = {0, 4, upper, lower};

    int fx = sendto(fd,message,strlen(message),0,(struct sockaddr*)&server_addr,sizeof(server_addr));
}

// Send out an error packet given an error code and an error message
void sendERR(int code, char* str) {
    char* message = NULL;
    size_t size = 0;
    int index = 0;

    // Set up size of message string
    // Add 5 at the end for error code, opcode, and null char
    size += strlen(str) + 5;

    // Allocate space for the message
    // Keep trying up to 10 times or until memory is allocated
    int count = 0;
    message = realloc(message, size);

    // Start building string starting with op code,
    // then error code, then error message
    message[index++] = 0;
    message[index++] = 5;
    message[index++] = 0;
    message[index++] = count ^ 65280;

    while (*str) {
        message[index++] = *str++;
    }

    message[index++] = '\0';
    
    // Send message to server
    sendto(fd,message,strlen(message),0,(struct sockaddr*)&server_addr,sizeof(server_addr))

    // Free memory for message
    free(message);
}

// Wait for ACK when expecting an ACK
int waitForACK(int expectedBlock) {
    int recvlen;
    bool notDone = true;
    char buff[516];
    memset( (char*)&buff,0,sizeof(buff));
    
    // Do not stop waiting until ack arrives
    // Mandatory for Stop-and-Wait
    while (notDone) {
        recvlen = recvfrom(fd,buff,sizeof(buff),0,(struct socketaddr*)&server_addr, sizeof(server_addr));
        
        if (recvlen > 0) {
            // If something is received but not an Ack,
            // Drop it & send error message (maybe not?)
            if (buff[1] != 4) {
                sendERR(0,"WAITING FOR ACK");

            // Otherwise, check if it's the right block
            // Then keep waiting if not, finish if so
            } else {
                int block = buff[2];
                block << 8;
                block += buff[3];

                if (block == expectedBlock) {
                    expectedBlock++;
                    notDone = true;
                }
            }
        }
    }

    // Return next block number
    return expectedBlock + 1;  
}
// TODO: Finish these two functions
// READING FROM SERVER AND WRITING TO CLIENT!
void readRQ(FILE* fp) {
    
}

// WRITING TO SERVER AND READING FROM CLIENT!
void writeRQ(FILE* fp) {

}

void readOrWrite(const char* filename , bool writeMode) {
    // ASSUMPTION: filename has been checked
    // See main() and validFileName()
    
    // WRITING TO SERVER = READING FROM CLIENT
    // READING FROM SERVER = WRITING TO CLIENT
    char* flag = writeMode ? "r" : "w"; 

    FILE* fp = fopen(filename,flag);

    if (writeMode) {
        writeRQ(fp);
    } else {
        readRQ(fp);
    }
}

// Help prompt showing usage and meaning of each flag
void showHelp() {
    char* str = "USAGE:\t./tftpclient -r filename\n\t\t./tftpclient -w filename\n\t\t./tftpclient -h\n";
    printf("%s",str);

    str = "\nFLAGS:\n\t-r:\tRequest server to read a file\n\t-w:\tRequest server to write a local file\n\t-h:\tShow this message\n\n";
    printf("%s",str);

    exit(-1);
}

int main (int argc, char const *argv[]) {
    bool writeMode;
    char *filename;

    // Handle -h (help) flag
    if ((argc > 0) && (strcmp(argv[1],"-h") == 0)) {
        showHelp();

    // Ensure there are only two arguments
    } else if (argc != 3) {
        printf("Incorrect number of arguments.\n");
        showHelp();

    // Set writeMode based on flag (false if -r)          
    } else if (strcmp(argv[1],"-r") == 0) {
        writeMode = false;
 
    // (true if -w)
    } else if (strcmp(argv[1],"-w") == 0) {
        writeMode = true;

    // Make sure filename is valid (no pathways)
    } else if (!validFileName(argv[2])) {
        printf("Invalid file name. File must be in the same directory as the server.\n");

    // If -r or -w cases aren't true, then something is wrong
    } else {
        printf("Incorrect usage.\n");
        showHelp();
    }

    // Set filename
    filename = argv[2];

    // Set up UDP socket
    fd = socket(AF_INET,SOCK_DGRAM,0);
    if (fd < 0) {
        perror("fd is less than 0");
        exit(-1);
    }
   
    // Allocate resources for server_addr 
    memset( (char*)&server_addr,0,sizeof(server_addr));

    // Other things for server_addr
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Get IP address of local host via DNS lookup
    struct hostent* hp;
    hp = gethostbyname("localhost");

    // Set timeout handler
    signal(SIGALRM,timeoutHandler);
 
    // Start the actual reading/writing process      
    readOrWrite(filename,writeMode);
}
