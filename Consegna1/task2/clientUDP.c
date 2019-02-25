#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
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
	int stop = 0;
	ssize_t byteRecv; // Number of bytes received
	ssize_t byteSent; // Number of bytes to be sent
	size_t msgLen;
	socklen_t serv_size;
	char receivedData [MAX_BUF_SIZE]; // Data to be received
	char sendData [MAX_BUF_SIZE]; // Data to be sent
	sfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sfd < 0){
		perror("socket"); // Print error message
		exit(EXIT_FAILURE);
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	serv_size = sizeof(server_addr);
	while(!stop){
		printf("Insert message:\n");
		scanf("%s", sendData);
		printf("String going to be sent to server: %s\n", sendData);
		if(strcmp(sendData, "exit") == 0){
			stop = 1;
		}
		msgLen = countStrLen(sendData);
		byteSent = sendto(sfd, sendData, msgLen, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
		printf("Bytes sent to server: %zd\n", byteSent);
		
		byteRecv = recvfrom(sfd, receivedData, MAX_BUF_SIZE, 0, (struct
		sockaddr *) &server_addr, &serv_size);
		printf("Received from server: ");
		printData(receivedData, byteRecv);
		if(stop){
			printf("commucation close");		
		}
	} // End of while
	return 0;
}
