all: client server

client: tftpclient.c
	gcc -o client tftpclient.c

server: tftpserver.c
	gcc -o server tftpserver.c

clean:
	rm client server
