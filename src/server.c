#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <fcntl.h>  // for open
#include <unistd.h>
#include <err.h>
#include <errno.h>

#include "packet.h"

#define MAXMSGLEN 100
#define MAX_PATHNAME 256

void execute_request(char *buf, char *rt_msg) {
	packet *p = (packet *)buf, *q;
	int rv;
	switch (p->opcode) {
		// open
		case 1: {
			int flags;
			mode_t m;
			char *pathname;
			memcpy(&flags, p->param, sizeof(int));
			memcpy(&m, p->param + sizeof(int), sizeof(mode_t));
			memcpy(&pathname, p->param + sizeof(int) + sizeof(mode_t), MAX_PATHNAME);
			rv = open(pathname, flags, m);
			break;
		}
		// close
		case 2: {
			int fildes;
			memcpy(&fildes, p->param, sizeof(int));
			rv = close(fildes);
			break;
		}
		// write
		case 3: {
			int fildes;
			size_t nbyte;
			char *buf;
			memcpy(&fildes, p->param, sizeof(int));
			memcpy(&nbyte, p->param + sizeof(int), sizeof(size_t));
			memcpy(&buf, p->param + sizeof(int) + sizeof(size_t), nbyte);
			rv = write(fildes, buf, nbyte);
			break;
		}
	}
	q = malloc(sizeof(packet));
	q->opcode = 0;
	q->param = malloc(2 * sizeof(int));
	memcpy(q->param, &rv, sizeof(int));
	memcpy(q->param + sizeof(int), &errno, sizeof(int));
	rt_msg = (char *)&q;
}

int main(int argc, char**argv) {
	char *msg="Hello from server";
	char buf[MAXMSGLEN+1];
	char *serverport;
	unsigned short port;
	int sockfd, sessfd, rv, send_rv;
	struct sockaddr_in srv, cli;
	socklen_t sa_size;
	
	// Get environment variable indicating the port of the server
	serverport = getenv("serverport15440");
	if (serverport) port = (unsigned short)atoi(serverport);
	else port=15440;
	port = 15332; // For local test
	
	// Create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);	// TCP/IP socket
	if (sockfd<0) err(1, 0);			// in case of error
	
	// setup address structure to indicate server port
	memset(&srv, 0, sizeof(srv));			// clear it first
	srv.sin_family = AF_INET;			// IP family
	srv.sin_addr.s_addr = htonl(INADDR_ANY);	// don't care IP address
	srv.sin_port = htons(port);			// server port

	// bind to our port
	rv = bind(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
	if (rv<0) err(1,0);
	
	// start listening for connections
	rv = listen(sockfd, 5);  // listening for clients, queue up to 5
	if (rv<0) err(1,0);
	
	fprintf(stderr, "start listening\n");

	// main server loop, linearly handle clients one at a time, with unlimited times
	while(1) {
		// wait for next client, get session socket
		sa_size = sizeof(struct sockaddr_in);
		// accept() blocks until a client has connected
		sessfd = accept(sockfd, (struct sockaddr *)&cli, &sa_size);
		if (sessfd<0) err(1,0);
		
		// get messages and send replies to this client, until it goes away
		while ( (rv=recv(sessfd, buf, MAXMSGLEN, 0)) > 0) { // receive up to MAXMSGLEN bytes into buf
			buf[rv]=0;		// null terminate string to print
			printf("%s\n", buf);  // print the received messege
			
			execute_request(buf, msg);

			// send reply
			fprintf(stderr, "server replying to client\n");
			send_rv = send(sessfd, msg, strlen(msg), 0);
			if (send_rv<0) err(1,0);
		}

		// if received bytes < 0, either client closed connection, or error
		if (rv<0) err(1,0);
		close(sessfd);  // done with this client
	}
	
	fprintf(stderr, "server shutting down cleanly\n");
	// close socket
	close(sockfd);  // server is done

	return 0;
}

