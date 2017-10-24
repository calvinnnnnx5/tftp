# TFTP Client and Server
May become a simple FTP client and server soon

## Introduction
This is a simple TFTP client and server originally made for class as specified by RFC 1350 with minor modifications:

+ The data field is restricted to 512 bytes
+ Restricted to local use only
+ Client-side files are stored in ./clientFiles/
+ Server-side files are stored in ./serverFiles/

The client and server both implement Reliable Data Transfer using ACK packets, block numbers, and a Stop-and-Wait scheme.
There is currently no flow or congestion control. The connection must be predetermined; that is, the IP address of the 
server must be hardcoded into the client-side as do the port numbers for both ends.

---

## Future Modifications

Planned future modifications include:

+ Open to non-local use
+ Replace Stop-and-Wait scheme to a pipelined scheme 
  + Go-Back-N
  + Selective Repeat
+ Add some form of flow and congestion control
+ Files with paths and such are allowed (with necessary restrictions)
+ Files may be stored outside of ./clientFiles/ and ./serverFiles/
+ Allow user input for IP address/URL and port

---

## Usage

#### Using the Makefile

`make client`

To make just the server:


`make server`

To make both

`make all`

To remove all made files

`make clean`


#### Using the server

Just run the server:

`./server`


#### Using the client

To read a file from the server:

`./client -r filename`


To write a file to server:

`./client -w filename`

To show help message

`./client -h`


---

## Possible Errors

#### Reasons why an error has occurred:

+ The client is reading `filename` from the server, but `filename` already exists client-side
+ The client is writing `filename` to the server, but `filename` already exists server-side
+ `filename` is undefined and/or `NULL`
+ Either the server or client is trying to read or write a file without the necessary permissions
+ `filename` does not exist on the side being read
+ `filename` is directory or contains a pathway
+ A request ACK packet was not received by either end
  + Congested connection
  + Client or server is down
+ 10 consecutive timeouts have occurred and the client has terminated and the server reset, as programmed
+ Incorrect block number was received
+ Incorrect packet format
  + Data field too large
  + Filename field is empty 
  + The header just doesn't follow protocol 


