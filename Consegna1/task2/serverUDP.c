#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "myfunction.h"

#define MAX_BUF_SIZE 1024 // Maximum size of UDP messages
#define SERVER_PORT 1234 // Server port

int main(int argc, char *argv[]){
	struct sockaddr_in server_addr; // struct containing server address information
	struct sockaddr_in client_addr; // struct containing client address information
	int sfd; // Server socket filed descriptor
	int br; // Bind result
	int i;
	ssize_t byteRecv; // Number of bytes received
	ssize_t byteSent; // Number of bytes to be sent
	socklen_t cli_size;
	char receivedData [MAX_BUF_SIZE]; // Data to be received
	char sendData [MAX_BUF_SIZE]; // Data to be sent
	sfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sfd < 0){
		perror("socket"); // Print error message
		exit(EXIT_FAILURE);
	}
	// Initialize server address information
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT); // Convert to network byte order
	server_addr.sin_addr.s_addr = INADDR_ANY; // Bind to any address
	br = bind(sfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
	if (br < 0){
		perror("bind"); // Print error message
		exit(EXIT_FAILURE);
	}
	cli_size = sizeof(client_addr);
	for(;;){
		byteRecv = recvfrom(sfd, receivedData, MAX_BUF_SIZE, 0, (struct
		sockaddr *) &client_addr, &cli_size);
		if(byteRecv == -1){
			perror("recvfrom");
			exit(EXIT_FAILURE);
		}
		printf("Received data: ");
		printData(receivedData, byteRecv);
		if(strncmp(receivedData, "exit", byteRecv) == 0){
			printf("Command to stop server received\n");
			byteSent = sendto(sfd, "goodbey", 7, 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
		}
		convertToUpperCase(receivedData, byteRecv);
		printf("Response to be sent back to client: ");
		printData(receivedData, byteRecv);
		byteSent = sendto(sfd, receivedData, byteRecv, 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
		printf(" %d %d\n", byteSent, byteRecv);
		if(byteSent != byteRecv){
			perror("sendto");
			exit(EXIT_FAILURE);
		}
	} // End of for(;;)
	return 0;
}

