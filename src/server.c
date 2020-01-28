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

//#include "packet.h"

#define MAXMSGLEN 4096
#define MAX_PATHNAME 512

void execute_request(char *buf, char *rt_msg, int *msg_len) {
	//packet *p = (packet *)buf, *q;
	int rv, opcode;
	memcpy(&opcode, buf, sizeof(int));
	switch (opcode) {
		// open
		case 1: {
			int flags;
			mode_t m;
			char *pathname = (char *)malloc(MAX_PATHNAME);
			fprintf(stderr, "--[OPEN]\n");

			memcpy(&flags, buf + sizeof(int), sizeof(int));
			memcpy(&m, buf + 2 * sizeof(int), sizeof(mode_t));
			memcpy(pathname, buf + 2 * sizeof(int) + sizeof(mode_t), MAX_PATHNAME);
			fprintf(stderr, "flags: %d, m: %d, pathname: %s\n", flags, (int)m, pathname);
			rv = open(pathname, flags, m);
			fprintf(stderr, "rv: %d\n", rv);

			/*q = malloc(sizeof(packet));
			q->opcode = 0;
			q->param = malloc(2 * sizeof(int));*/
			memcpy(rt_msg, &rv, sizeof(int));
			memcpy(rt_msg + sizeof(int), &errno, sizeof(int));
			//rt_msg = (char *)&q;

			*msg_len = 2 * sizeof(int);
			break;
		}
		// close
		case 2: {
			int fildes;
			fprintf(stderr, "--[CLOSE]:\n");

			memcpy(&fildes, buf + sizeof(int), sizeof(int));
			fprintf(stderr, "fildes: %d\n", fildes);
			rv = close(fildes);
			fprintf(stderr, "rv: %d\n", rv);

			/*q = malloc(sizeof(packet));
			q->opcode = 0;
			q->param = malloc(2 * sizeof(int));*/
			memcpy(rt_msg, &rv, sizeof(int));
			memcpy(rt_msg + sizeof(int), &errno, sizeof(int));
			//rt_msg = (char *)&q;

			*msg_len = 2 * sizeof(int);
			break;
		}
		// write
		case 3: {
			int fildes;
			size_t nbyte;
			char *w_buf = (char *)malloc(MAXMSGLEN);
			fprintf(stderr, "--[WRITE]:\n");

			memcpy(&fildes, buf + sizeof(int), sizeof(int));
			memcpy(&nbyte, buf + 2 * sizeof(int), sizeof(size_t));
			memcpy(w_buf, buf + 2 * sizeof(int) + sizeof(size_t), nbyte);
			fprintf(stderr, "fildes: %d, nbyte: %d, buf: %s\n", fildes, (int)nbyte, w_buf);
			rv = write(fildes, w_buf, nbyte);
			fprintf(stderr, "rv: %d\n", rv);

			/*q = malloc(sizeof(packet));
			q->opcode = 0;
			q->param = malloc(2 * sizeof(int));*/
			memcpy(rt_msg, &rv, sizeof(int));
			memcpy(rt_msg + sizeof(int), &errno, sizeof(int));
			//rt_msg = (char *)&q;

			*msg_len = 2 * sizeof(int);
			break;
		}
		default:
			fprintf(stderr, "Default\n");
			char *msg = "Hello from server";
			memcpy(rt_msg, msg, strlen(msg));

			*msg_len = strlen(msg);
			break;
	}
}

int main(int argc, char**argv) {
	char *msg;
	char buf[MAXMSGLEN+1];
	char *serverport;
	unsigned short port;
	int sockfd, sessfd, rv, send_rv, msg_len;
	struct sockaddr_in srv, cli;
	socklen_t sa_size;
	
	// Get environment variable indicating the port of the server
	serverport = getenv("serverport15440");
	if (serverport) port = (unsigned short)atoi(serverport);
	else port=15440;
	//port = 15228; // For local test
	
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
	
	// main server loop, linearly handle clients one at a time, with unlimited times
	while(1) {
		fprintf(stderr, "Waiting for client\n");
		// wait for next client, get session socket
		sa_size = sizeof(struct sockaddr_in);
		// accept() blocks until a client has connected
		sessfd = accept(sockfd, (struct sockaddr *)&cli, &sa_size);
		if (sessfd<0) err(1,0);
		
		// get messages and send replies to this client, until it goes away
		while ( (rv=recv(sessfd, buf, MAXMSGLEN, 0)) > 0) { // receive up to MAXMSGLEN bytes into buf
			buf[rv]=0;		// null terminate string to print
			fprintf(stderr, "received msg: ");
			printf("%s\n", buf);  // print the received messege
			
			msg = malloc(MAXMSGLEN);
			execute_request(buf, msg, &msg_len);

			// send reply
			fprintf(stderr, "server replying to client\n");
			send_rv = send(sessfd, msg, msg_len, 0);
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

