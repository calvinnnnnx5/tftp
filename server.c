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

void timeout(int x) {
    if (x == SIGALRM) {
        bool notDone = true;

        //Exit/terminate after 10 consecutive tries
        if (!sendAttempts) {
            exit(-1);
	}

        //If unacknowledged & less than 10 consecutive tries,
        //resend packet, double timer, and decrement sendAttempts
        else {
            //resend packet
            resend();

            //reset timer
            timeoutInterval = timeoutInterval * 2;
            alarm(timeoutInterval);

            //decrease sendAttempts by 1
            sendAttempts--;
        }
    }
}

bool validFileName(const char fileName[512], bool writeMode) {
    int i = 0;

    // Make sure filename doesn't includepaths
    // All files are in same directory
    while(fileName[i] != '\0') {
        if (fileName[i] == '/') {
            return false;
        }
        i++;
    }

    // If no errors, return true
    return true;
}

void resend() {
    sendto(fd,lastPacket,strlen(lastPacket),0,(struct sockaddr*)&client_addr,sizeof(client_addr));
}

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
    char* ptr = data;
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
}

int waitForACK(int expectedBlock) {
    int recvlen;
    bool notDone = true;
    char buff[5];
    memset( (char*)&buff,0,sizeof(buff));
    
    // Do not stop waiting until ack arrives
    // Mandatory for Stop-and-Wait
    while (notDone) {
        recvlen = recvfrom(fd,buff,sizeof(buff),0,(struct socketaddr*)&client_addr, sizeof(client_addr));
       
        if (recvlen > 4) {
            printf("ACK packet it too big. Should be 4 bytes. Terminating connection.\n");
	    free(message);
	    exit(-1);
	}
 
        if (recvlen > 0) {
            int block = buff[2];
            block << 8;
            block += buff[3];

            // If block == expectedBlock, then ACK message received
            if (block == expectedBlock) {
                expectedBlock++;
                notDone = true;
	    }
            // If block > expectedBlock, something went wrong
            else if (block > expectedBlock) {
                printf("Packets are out of order. Terminating connection.\n");
		free(message);
		exit(-1);
            } 
	    // In case of block < expected Block, do nothing
            // Duped ack => no action as stated in writeup
            }
        }
    }

    // Return next block number
    return expectedBlock;  
}

// Reading from client and writing to server
void readFromClient(FILE* fp) {
    int blockNumber = 1;
    bool notDone = true;
    char buff[517] = {0};

    // Now in receiving mode
    while (notDone) {
        recvlen = recvfrom(fd,buff,sizeof(buff),0,(struct socketaddr*)&client_addr, sizeof(client_addr)); 
        
        // Handle packet received
        if (recvlen > 0 && recvlen < 517) {
            int block = buff[2];
            block << 8;
            block += buff[3];

            // If packet is a data packet with the correct block number
            // Write to file specified by fp
            if ((buff[1] == 3) && (block == blockNumber)) {
                
                // Turn off timer for request packet
                alarm(0);

                // Set ptr to point to beginning of Data field 
                char* ptr = buff;
                ptr += 4;

                // Length of data field
                int length = recvlen - 4;

                for (int i = 0; i < length; i++) {
                    checkERR(false);
                    fputc(*ptr++,fp);
                }

                // If the number of bytes received is less than a full packet
                // Then transmission is done. Just exit the while loop.
                if (recvlen < 516) {
                    notDone = false;
                    fclose(fp);
                }

                // Send ACK with block number received
                sendACK(blockNumber++, false);
            }

            // In case of duplicate data packet, resend last packet (ACK message)
	    else if ((buff[1] == 3) && (block < blockNumber)) {
                resendPacket();

            // A bunch of possible errors
            } 
	    else if (block != blockNumber) {
                printf("Did not receive block number 1 after RRQ ACK. Terminating connection.\n");
		exit(-1);
	    } 
	    else if (buff[1] == 5) {
                exit(-1);
            } 
	}
 
        // Data field must be limited to 512 bytes. 
        // When Data > 512 bytes, packet > 516 Bytes.
        // (4 bytes for opcode and block number, 512 bytes for data) 
	else if (recvlen < 516) {
            printf("Received packet is too large. Terminating connection.\n");
	    exit(-1);
        }
    }
}

// Writing to client from server
void writeToClient(FILE* fp) {

    // Set up block number (1 for WRQ), wait for ACK or timeout
    blockNumber = 1;

    // Set up buffer, read first 512 characters from the file
    char buff[512] = {0};
    int bytesRead = fread(buff,1,512,fp);

    // While bytesRead is not 0, keep sending packets until 
    // either bytesRead is 0 or the end of file has been reached
    while (bytesRead > 0)  {
        if (bytesRead < 512) {
            buff[bytesRead] = '\0';
        }   

        sendData(buff,blockNumber);

        blockNumber = waitForACK(blockNumber);
        bytesRead = fread(buff,1,512,fp);      
    }

    fclose(fp);
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
