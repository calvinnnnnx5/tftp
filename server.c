#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#define PORT 6112
//#define PORT 12345
#define CLIENTPORT 54321

typedef int bool;
#define true 1
#define false 0

static int sendAttempts = 10;
static const int TIMEOUT_BASE = 1;
static int timeoutInterval = 1;
static char*  lastPacket;
static struct sockaddr_in server_addr;
static struct sockaddr_in client_addr;

static int fd = -1;

void resendPacket() {
    sendto(fd,lastPacket,strlen(lastPacket),0,(struct sockaddr*)&client_addr,sizeof(client_addr));
}

void timeoutHandler(int x) {

    if (x == SIGALRM) {

        //Exit/terminate after 10 consecutive tries
        if (!sendAttempts) {
            printf("TIMEOUT: 0/10 send attempts remaining. Terminating.\n");
            exit(-1);

        //If still unacknowledged & less than 10 consecutive tries,
        //resend packet again, double timer, & decrement sendAttempts
        } else {
            printf("TIMEOUT: %d/10 send attempts remaining. Resending last packet with interval %d s.\n",sendAttempts,timeoutInterval);

            //resend packet
            resendPacket();

            //reset timer
            //timeoutInterval = (timeoutInterval < 16) ? timeoutInterval * 2 : 16;
            alarm(timeoutInterval);

            //decrease sendAttempts by 1
            sendAttempts--;
        }
    }
}


// Send out an error packet given an error code and an error message
void sendERR(int code, char* str, char* printstr) {

    // Print message to STDOUT
    printf("%s",printstr);

    char* message = NULL;
    size_t size = 0;
    int index = 0;

    // Set up size of message string
    // Add 5 at the end for error code, opcode, and null char
    size += strlen(str) + 5;

    // Allocate space for the message
    // Keep trying up to 10 times or until memory is allocated
    message = realloc(message, size);

    // Start building string starting with op code,
    // then error code, then error message
    message[index++] = 0;
    message[index++] = 5;
    message[index++] = 0;
    message[index++] = code ^ 65280;

    while (*str) {
        message[index++] = *str++;
    }

    message[index++] = '\0';
    
    // Send message to server
    sendto(fd,message,strlen(message),0,(struct sockaddr*)&server_addr,sizeof(server_addr));

    // Free memory for message
    free(message);

    // Terminate early in case of an error
    exit(-1);
}

// Check for possible I/O errors
void checkERR(bool writeMode) {

    // Possible errors shared by both modes
    switch(errno) {
        case 5:     sendERR(0,"I/O ERROR", "I/O ERROR. Terminating.\n"); 
        case 13:    sendERR(2,"", "ACCESS VIOLATION. Terminating.\n");
    }

    // Possible errors specific to writng mode
    if (writeMode) {
        switch(errno) {
            case 2:     sendERR(1,"", "FILE NOT FOUND. Terminating.\n");
        }

    // Possible errors specific to reading mode
    } else {
        switch(errno) {
            case 12:
            case 28:    sendERR(3,"","OUT OF MEMORY. Terminating.\n");
            case 17:    sendERR(6,"","FILE ALREADY EXISTS. Terminating.\n");
            case 30:    sendERR(2,"","FILE IS READ-ONLY. Terminating.\n");
        }
    }
}

// Make sure the filename/string is valid as specified by project writeup
bool validFileName(const char* fileName) {
    int i = 0;

    // Make sure filename includes NO paths
    // All files read & written must be in same directory
    while(fileName[i] != '\0') {
        if (fileName[i] == '/') {
            return false;
        }
        i++;
    }
}

// Make sure the file exists in /serverFiles/
bool validFile(const char* fileName, bool writeMode) {

    // Make sure, if we are writing to server, 
    // that the file exists and can be read
    if (writeMode) {
        int res = access(fileName,R_OK);
        if (res != 0) {
            checkERR(true);
        }      
 
    // Otherwise, we are reading from server
    // And must check if there is already a file
    // with the same name in the same directory
    } else  {
        int res = access(fileName, F_OK);
        if (res != 0) {
            checkERR(false);
        }
    }

    // If no possible errors, return true
    return true;
}

// Creates the final packet with given data & sends it out
int sendData(const char* data, int blockNumber) {
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
    char* ptr = NULL;
    ptr = (char *)data;
    while(*ptr) {
        message[index++] = *ptr++;
    }

    // Send message
    int bytesSent =  sendto(fd,message,strlen(message),0,(struct sockaddr*)&client_addr,sizeof(client_addr));     

    // Set timeout timer
    timeoutInterval = TIMEOUT_BASE;
    alarm(timeoutInterval);

    // Set lastPacket to message in case retransmission is necessary
    strncpy(lastPacket,message,size);

    // Free message to prevent memory leaks
    free(message);

    // Return number of bytes sent
    return bytesSent;
}

void sendACK(int blockNumber, bool writeMode) {

    // Get the upper half of blockNumber (shift right by 8);
    char upper = blockNumber >> 8;

    // Let blockNumber be XXXX XXXX XXXX XXXX
    // Then blockNumber ^ 65280 = XXXX XXXX XXXX XXXX ^ 1111 1111 0000 0000
    // i.e. Get the lower half of blockNumber
    char lower = blockNumber ^ 65280;

    char message[4] = {0, 4, upper, lower};

    int fx = sendto(fd,message,strlen(message),0,(struct sockaddr*)&client_addr,sizeof(client_addr));

    // In case no new data packet is received
    // while in read mode, resend the ACK packet
    if (!writeMode) {
        timeoutInterval = TIMEOUT_BASE;
        alarm(timeoutInterval);
        strncpy(lastPacket,message,4);
    }
}

// Wait for ACK when expecting an ACK
// Should only be used after initial R/WRQ
// Then only in writing mode (ACK for data sent)
int waitForACK(int expectedBlock) {
    int recvlen;
    bool notDone = true;
    char buff[5];
    memset( (char*)&buff,0,sizeof(buff));
    int len = sizeof(client_addr);
    
    // Do not stop waiting until ack arrives
    // Mandatory for Stop-and-Wait
    printf("WAITING FOR ACK OF BLOCK NUMBER %d.\n", expectedBlock);
    while (notDone) {
        recvlen = recvfrom(fd,buff,sizeof(buff),0,(struct sockaddr*)&client_addr,&len);
       
        if (recvlen > 4) {
            sendERR(0,"ACK PACKET TOO LARGE; MUST BE 4 BYTES", "ACK PACKET TOO LARGE; MUST BE 4 BYTES. Terminating.\n");
        }
 
        if (recvlen > 0) {
            // If something is received but not an Ack,
            // Drop it & send error message (maybe not?)
            if (buff[1] != 4) {
                sendERR(0,"WAITING FOR ACK", "WAITING FOR ACK. Terminating.\n");

            // Otherwise, check if it's the right block
            // Then keep waiting if not, finish if so
            } else {
                int block = buff[2];
                block << 8;
                block += buff[3];

                // If block == expectedBlock, then ACK message received
                if (block == expectedBlock) {
                    expectedBlock++;
                    notDone = true;
                    printf("RECV: ACK of Block Number %d.\n",block);

                // If block > expectedBlock, there is something obv wrong
                } else if (block > expectedBlock) {
                    sendERR(0,"RECEIVED BLOCK NUMBER GREATER THAN EXPECTED","RECIEVED BLOCK NUMBER GREATER THAN EXPECTED. Terminating.\n");

                // In case of block < expected Block, do nothing
                // Duped ack => no action as stated in writeup
                } else {
                    printf("RECV: Duplicate ACK message of Block Number %d. No action taken.\n", block);
                }
            }
        }
    }

    // Return next block number
    return expectedBlock;  
}

void sendRQ(const char* filename, bool writeMode) {
    char opcode = writeMode ? 2 : 1;
    char *mode = "octet";
    char *rqPacket = NULL; 
    char *fnPtr = NULL;
    fnPtr = (char*)filename;
    int index = 0;
    
    // Get size for rqPacket
    // size of filename + 2 bytes for opcode
    // + 2 bytes for null chars
    // + length of "octet" (required mode)
    size_t size = strlen(filename) +  2 + 2 + strlen(mode);  

    // Allocate space for rqPacket
    rqPacket = realloc(rqPacket, size); 

    // Set appropriate fields in order
    rqPacket[index++] = 0;
    rqPacket[index++] = opcode;
 
    while (*fnPtr) {
        rqPacket[index++] = *fnPtr++;
    }
    rqPacket[index++] = '\0';

    while (*mode) {
        rqPacket[index++] = *mode++;
    }
    rqPacket[index++] = '\0';
    
    // Copy rqPacket to lastPacket in case
    // of timeout occurring
    lastPacket = realloc(lastPacket,size);
    strncpy(lastPacket,rqPacket,size);
    
    // Send request packet
    sendto(fd,rqPacket,strlen(rqPacket),0,(struct sockaddr*)&server_addr,sizeof(server_addr));

    // Set timer
    alarm(timeoutInterval);    
    
    // Free memory
    free(rqPacket);
}

// READING FROM CLIENT AND WRITING TO SERVER!
void readFromClient(FILE* fp) {
    int blockNumber = 0;
    bool notDone = true;
    char buff[517] = {0};
    int len = sizeof(client_addr);

    // Send an ACK back to client
    sendACK(blockNumber,true);

    // Now in receiving mode
    printf("Waiting for Data Packets from server...\n");
    while (notDone) {
        int recvlen = recvfrom(fd,buff,sizeof(buff),0,(struct sockaddr*)&client_addr,&len); 
        
        // Handle packet received
        if (recvlen > 0 && recvlen < 517) {
            int block = buff[2];
            block << 8;
            block += buff[3];

            // If packet is a data packet with the correct block number
            // Write to file specified by fp
            if ((buff[1] == 3) && (block == blockNumber)) {
                
                printf("RECV: Data Packet of Block Number %d.\n",block);

                // Set ptr to point to beginning of Data field 
                char* ptr = buff;
                ptr += 4;

                // Length of data field
                int length = recvlen - 4;

                while (length > 0) {
                    checkERR(false);
                    fputc(*ptr++,fp);
                    length--;
                }

                // If the number of bytes received is less than a full packet
                // Then transmission is done. Just exit the while loop.
                if (recvlen < 516) {
                    notDone = false;
                    fclose(fp);
                }

                // Send ACK with block number received
                printf("SEND: ACK of Block Number %d.\n",blockNumber);
                sendACK(blockNumber++, true);
                
            // In case of duplicate data packet, resend last packet (ACK message)
            } else if ((buff[1] == 3) && (block < blockNumber)) {
                printf("RECV: Duplicate data packet of block %d.\n",block);
                printf("SEND: Last ACK message of block %d.\n",blockNumber - 1);   
                resendPacket();

            // A bunch of possible errors
            }  else if (block > blockNumber) {
                sendERR(0,"RECEIVED A BLOCK GREATER THAN EXPECTED","RECEIVED A BLOCK GREATER THAN EXPECTED. Terminating.\n");

            } else if (buff[1] == 5) {
                exit(-1);

            } else {
                sendERR(0,"PACKET RECEIVED WAS NOT ACK AFTER RRQ", "PACKET RECEIVED WAS NOT ACK AFTER RRQ. Terminating\n");
            }
 
        // Data field must be limited to 512 bytes. 
        // When Data > 512 bytes, packet > 516 Bytes.
        // (4 bytes for opcode and block number, 512 bytes for data)
        } else if (recvlen < 516) {
            sendERR(0,"DATA TOO LARGE; RESTRICT TO 512 BYTES", "RECEIVED DATA EXCEEDS 512 BYTES. Terminating.\n");
        }
    }
    
    fclose(fp);
}

// WRITING TO CLIENT AND READING FROM SERVER!
void writeToClient(FILE* fp) {

    // Set up block number (0 for WRQ), wait for ACK or timeout
    int blockNumber = 1;

    // Set up buffer, read first 512 characters from the file
    char buff[512] = {0};
    int bytesRead = fread(buff,1,512,fp);

    // While bytesRead is not 0, keep sending packets until 
    // either bytesRead is 0 or the end of file has been reached
    while (bytesRead > 0)  {
        if (bytesRead < 512) {
            buff[bytesRead] = '\0';
        }   
        
        printf("SEND: Data Packet of size %d with Block Number %d.\n",bytesRead,blockNumber);
        sendData(buff,blockNumber);

        blockNumber = waitForACK(blockNumber);
        bytesRead = fread(buff,1,512,fp);      
    }

    // That's it.
    fclose(fp);
}

void readOrWrite(const char* filename, bool writeMode) {
    // ASSUMPTION: filename has been checked
    // See main() and validFileName()

    // WRITING TO SERVER = READING FROM CLIENT
    // READING FROM SERVER = WRITING TO CLIENT
    char* flag = writeMode ? "w" : "r"; 

    FILE* fp = fopen(filename,flag);
    checkERR(writeMode);

    if (writeMode) {
        printf("WRQ received by server.\n");
        printf("Reading from %s to client.\n",filename); 
       readFromClient(fp);
    } else {
        printf("RRQ received by server.\n");
        printf("Writing to %s from client.\n",filename);
        writeToClient(fp);
    }
}

// Help prompt showing usage and meaning of each flag
void showHelp() {
    char* str = "USAGE:\t./tftpclient -r filename\n\t./tftpclient -w filename\n\t./tftpclient -h\n";
    printf("%s",str);

    str = "\nFLAGS:\t-r:\tRequest server to read a file\n\t-w:\tRequest server to write a local file\n\t-h:\tShow this message\n\n";
    printf("%s",str);

    exit(-1);
}

int main (int argc, const char *argv[]) {
    bool writeMode;
    const char *filename;

    // Handle -h (help) flag
    if ((argc > 1) && (strcmp(argv[1],"-h") == 0)) {
        showHelp();
    }

    // Set up UDP socket
    fd = socket(AF_INET,SOCK_DGRAM,0);
    if (fd < 0) {
        perror("fd is less than 0");
        exit(-1);
    }
  
    // Get IP address of local host via DNS lookup
    struct hostent* hp;
    hp = gethostbyname("localhost");

    // Allocate resources for server & client addr
    memset( (char*)&server_addr,0,sizeof(server_addr));
    memset( (char*)&client_addr,0,sizeof(client_addr));
    
    // Other things for server & client addr
    server_addr.sin_family = AF_INET;
    client_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    client_addr.sin_port = htons(CLIENTPORT);
    memcpy((void*)&server_addr.sin_addr,hp->h_addr_list[0],hp->h_length);
    memcpy((void*)&client_addr.sin_addr,hp->h_addr_list[0],hp->h_length);
                                     

    // Bind socket to port/process
    if (bind(fd, (struct sockaddr*)&server_addr,sizeof(server_addr)) ) {
        printf("UNABLE TO BIND PORT %d TO PROCESS SOCKET. Terminating.\n",PORT);
        exit(-1);
    }

    // Set timeout handler
    signal(SIGALRM,timeoutHandler);
 
    int client_size = sizeof(client_addr);
    char buff[5] = {0};

    printf("Waiting for a request.\n");
    while (true) {
        
        int bytesReceived = recvfrom(fd,buff,sizeof(buff),0,(struct sockaddr*)&client_addr,&client_size);

        if (bytesReceived > 0) {

            if ((buff[1] == 1) || (buff[1] == 2)) {
                char* fnPtr = buff;
                char* filename;
                char* fn;
                size_t nameSize = 0;
                fnPtr += 2;
                 
                // writeMode is true if packet is WRQ
                bool writeMode = buff[1] == 2;

                while (*fnPtr) {
                    nameSize++;
                    fn = realloc(fn,nameSize+1);
                    fn[nameSize-1] = *fnPtr++;
                }
                fn[nameSize] = '\0';

                char* mode;
                nameSize = 0;
                fnPtr++;

                while (*fnPtr) {
                    nameSize++;
                    mode = realloc(mode,nameSize+1);
                    mode[nameSize-1] = *fnPtr++;
                }
                mode[nameSize] = '\0';

                if (strcmp(mode,"octet") != 0) {
                    sendERR(0,"ERROR: INCORRECT MODE.","ERROR: INCORRECT MODE. Terminating.\n");
                } else {
                    validFileName(fn);
                }

                size_t dirLen = strlen("/serverFiles/");
                filename = realloc(filename,dirLen + strlen(fn) + 1);
                strncpy(filename,"/serverFiles/",dirLen);
                strcat(filename,fn);

                // Make sure file exists in /serverFiles/
                if (validFile(filename,writeMode)) {
                    readOrWrite(filename,writeMode);

                // If not, send ERR
                } else {
                    sendERR(1,"ERROR: FILE NOT FOUND","ERROR: FILE NOT FOUND. Terminating.\n");
                }

            } else {
                sendERR(4,"","ERROR: Illegal operation. Received a packet with opcode %d instead of RQ. Terminating.\n");
            }

            if (writeMode) {
                printf("Write of file %s to client successful. Waiting for new request.\n\n",filename);
            } else {
                printf("Read of file %s from client successful. Waiting for new request.\n\n",filename);
            }

            printf("Waiting for a new request.");
        } else if (bytesReceived < 0) {
            sendERR(0,"ERROR: -1 RETURNED FROM recvfrom().", "ERROR: -1 RETURNED FROM recvfrom(). Terminating.\n");
        }
    }
}
