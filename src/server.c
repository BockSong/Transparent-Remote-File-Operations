#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <dirtree.h>

#define MAXMSGLEN 4096
#define MAX_PATHNAME 512
#define FD_OFFSET 2000

int fd_table[FD_OFFSET]; // seems that we don't need this

int pack_tree(struct dirtreenode *sub, char *sub_send) {
	fprintf(stderr, "Starting packing tree ... \n");
	int sub_length = MAX_PATHNAME + sizeof(int), i;
	sub_send = (char *)malloc(sub_length);
	memcpy(sub_send, sub->name, MAX_PATHNAME);
	memcpy(sub_send + MAX_PATHNAME, &(sub->num_subdirs), sizeof(int));
	for (i = 0; i < sub->num_subdirs; i++) {
		fprintf(stderr, "Get in subdirs: %d, length: %d\n", i, sub_length);
		sub_length += pack_tree(sub->subdirs[i], sub_send + sub_length);
	}
	fprintf(stderr, "Safely ended, length: %d\n", sub_length);
	return sub_length;
}

void execute_request(char *buf, char *rt_msg, int *msg_len, int sessfd) {
	int opcode;
	memcpy(&opcode, buf, sizeof(int));
	switch (opcode) {
		// open
		case 1: {
			int rv, flags;
			mode_t m;
			char *pathname = (char *)malloc(MAX_PATHNAME);

			memcpy(&flags, buf + sizeof(int), sizeof(int));
			memcpy(&m, buf + 2 * sizeof(int), sizeof(mode_t));
			memcpy(pathname, buf + 2 * sizeof(int) + sizeof(mode_t), MAX_PATHNAME);
			rv = open(pathname, flags, m);
			if (rv != -1) {
				fd_table[rv] = sessfd;
			}

			fprintf(stderr, "--[OPEN]\n");
			fprintf(stderr, "flags: %d, m: %d, pathname: %s\n", flags, (int)m, pathname);
			fprintf(stderr, "rv: %d\n", rv);

			memcpy(rt_msg, &errno, sizeof(int));
			memcpy(rt_msg + sizeof(int), &rv, sizeof(int));
			*msg_len = 2 * sizeof(int);
			break;
		}
		// close
		case 2: {
			int rv, fildes;
			memcpy(&fildes, buf + sizeof(int), sizeof(int));

			// fd access control for multi-client security
			if (fd_table[fildes] != sessfd) {
				fprintf(stderr, "Attempt to use unavailable fd !\n");
				rv = -1;
			}
			else {
				rv = close(fildes);
				fd_table[fildes] = -1;
			}

			fprintf(stderr, "--[CLOSE]:\n");
			fprintf(stderr, "fildes: %d\n", fildes);
			fprintf(stderr, "rv: %d\n", rv);

			memcpy(rt_msg, &errno, sizeof(int));
			memcpy(rt_msg + sizeof(int), &rv, sizeof(int));
			*msg_len = 2 * sizeof(int);
			break;
		}
		// write
		case 3: {
			int fildes;
			size_t rv, nbyte;
			char *w_buf;

			memcpy(&fildes, buf + sizeof(int), sizeof(int));
			memcpy(&nbyte, buf + 2 * sizeof(int), sizeof(size_t));
			w_buf = (char *)malloc(nbyte);
			memcpy(w_buf, buf + 2 * sizeof(int) + sizeof(size_t), nbyte);

			// fd access control for multi-client security
			if (fd_table[fildes] != sessfd)
				rv = -1;
			else
				rv = write(fildes, w_buf, nbyte);

			fprintf(stderr, "--[WRITE]:\n");
			fprintf(stderr, "fildes: %d, nbyte: %d, buf: %s\n", fildes, (int)nbyte, w_buf);
			fprintf(stderr, "rv: %d\n", (int)rv);

			memcpy(rt_msg, &errno, sizeof(int));
			memcpy(rt_msg + sizeof(int), &rv, sizeof(int));
			*msg_len = sizeof(int) + sizeof(size_t);
			break;
		}
		// read
		case 4: {
			int fildes;
			size_t rv, nbyte;
			char *r_buf;

			memcpy(&fildes, buf + sizeof(int), sizeof(int));
			memcpy(&nbyte, buf + 2 * sizeof(int), sizeof(size_t));
			r_buf = (char *)malloc(nbyte);

			// fd access control for multi-client security
			if (fd_table[fildes] != sessfd)
				rv = -1;
			else
				rv = read(fildes, r_buf, nbyte);

			fprintf(stderr, "--[READ]:\n");
			fprintf(stderr, "fildes: %d, nbyte: %d, buf: %s\n", fildes, (int)nbyte, r_buf);
			fprintf(stderr, "rv: %d\n", (int)rv);

			memcpy(rt_msg, &errno, sizeof(int));
			memcpy(rt_msg + sizeof(int), &rv, sizeof(int));
			memcpy(rt_msg + 2 * sizeof(int), r_buf, nbyte);
			*msg_len = sizeof(int) + sizeof(size_t) + nbyte;
			break;
		}
		// lseek
		case 5: {
			int fildes, whence;
			off_t rv, offset;

			memcpy(&fildes, buf + sizeof(int), sizeof(int));
			memcpy(&offset, buf + 2 * sizeof(int), sizeof(off_t));
			memcpy(&whence, buf + 2 * sizeof(int) + sizeof(off_t), sizeof(int));

			// fd access control for multi-client security
			if (fd_table[fildes] != sessfd)
				rv = -1;
			else
				rv = lseek(fildes, offset, whence);

			fprintf(stderr, "--[LSEEK]:\n");
			fprintf(stderr, "fildes: %d, offset: %d, whence: %d\n", fildes, (int)offset, whence);
			fprintf(stderr, "rv: %d\n", (int)rv);

			memcpy(rt_msg, &errno, sizeof(int));
			memcpy(rt_msg + sizeof(int), &rv, sizeof(int));
			*msg_len = sizeof(int) + sizeof(off_t);
			break;
		}
		// stat
		case 6: {
			int rv;
			struct stat *s_buf = (struct stat *)malloc(sizeof(struct stat));
			char *pathname = (char *)malloc(MAX_PATHNAME);

			memcpy(pathname, buf + sizeof(int), MAX_PATHNAME);
			rv = stat(pathname, s_buf);
			fprintf(stderr, "--[STAT]\n");
			fprintf(stderr, "pathname: %s\n", pathname);
			fprintf(stderr, "rv: %d\n", rv);

			memcpy(rt_msg, &errno, sizeof(int));
			memcpy(rt_msg + sizeof(int), &rv, sizeof(int));
			memcpy(rt_msg + 2 * sizeof(int), s_buf, sizeof(struct stat));
			*msg_len = 2 * sizeof(int) + sizeof(struct stat);
			break;
		}
		// __xstat
		case 7: {
			int rv, ver;
			struct stat *s_buf = (struct stat *)malloc(sizeof(struct stat));
			char *pathname = (char *)malloc(MAX_PATHNAME);

			memcpy(&ver, buf + sizeof(int), sizeof(int));
			memcpy(pathname, buf + 2 * sizeof(int), MAX_PATHNAME);
			rv = __xstat(ver, pathname, s_buf);

			fprintf(stderr, "--[__XSTAT]\n");
			fprintf(stderr, "ver: %d, pathname: %s\n", ver, pathname);
			fprintf(stderr, "rv: %d\n", rv);

			memcpy(rt_msg, &errno, sizeof(int));
			memcpy(rt_msg + sizeof(int), &rv, sizeof(int));
			memcpy(rt_msg + 2 * sizeof(int), s_buf, sizeof(struct stat));
			*msg_len = 2 * sizeof(int) + sizeof(struct stat);
			break;
		}
		// unlink
		case 8: {
			int rv;
			char *pathname = (char *)malloc(MAX_PATHNAME);

			memcpy(pathname, buf + sizeof(int), MAX_PATHNAME);
			rv = unlink(pathname);

			fprintf(stderr, "--[UNLINK]\n");
			fprintf(stderr, "pathname: %s\n", pathname);
			fprintf(stderr, "rv: %d\n", rv);

			memcpy(rt_msg, &errno, sizeof(int));
			memcpy(rt_msg + sizeof(int), &rv, sizeof(int));
			*msg_len = 2 * sizeof(int);
			break;
		}
		// getdirentries
		case 9: {
			int fd, rv, nbytes;
			long *basep = (long *)malloc(sizeof(long));
			char *g_buf;

			memcpy(&fd, buf + sizeof(int), sizeof(int));
			memcpy(&nbytes, buf + 2 * sizeof(int), sizeof(int));
			memcpy(basep, buf + 3 * sizeof(int), sizeof(long));
			g_buf = (char *)malloc(nbytes);

			// fd access control for multi-client security
			if (fd_table[fd] != sessfd)
				rv = -1;
			else
				rv = getdirentries(fd, g_buf, nbytes, basep);

			fprintf(stderr, "--[getdirentries]:\n");
			fprintf(stderr, "fildes: %d, nbyte: %d\n", fd, nbytes);
			fprintf(stderr, "rv: %d\n", (int)rv);

			memcpy(rt_msg, &errno, sizeof(int));
			memcpy(rt_msg + sizeof(int), &rv, sizeof(int));
			memcpy(rt_msg + 2 * sizeof(int), g_buf, nbytes);
			*msg_len = 2 * sizeof(int) + nbytes;
			break;
		}
		// getdirtree
		case 10: {
			int rt_length = 0;
			struct dirtreenode *rv;
			char *pathname = (char *)malloc(MAX_PATHNAME), *rv_send = NULL;

			fprintf(stderr, "--[getdirtree]\n");
			memcpy(pathname, buf + sizeof(int), MAX_PATHNAME);
			rv = getdirtree(pathname);
			rt_length = pack_tree(rv, rv_send);

			fprintf(stderr, "--------------\n");
			fprintf(stderr, "length: %d, subdirs: %d, pathname: %s\n", rt_length, rv->num_subdirs, pathname);

			memcpy(rt_msg, &errno, sizeof(int));
			memcpy(rt_msg + sizeof(int), &rt_length, sizeof(int));
			// TODO: So weird bug here. send rv_send will make previous rt_length become 0 ...
			memcpy(rt_msg + 2 * sizeof(int), rv_send, rt_length);
			*msg_len = 2 * sizeof(int) + rt_length;

			// no longer need the tree at server, free it at once
			freedirtree(rv);
			break;
		}
		// freedirtree: nothing to do for rpc
		default:
			fprintf(stderr, "Default\n");
			char *msg = "Default msg from server";
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
	int sockfd, sessfd, rv, send_rv, msg_len, pid;
	struct sockaddr_in srv, cli;
	socklen_t sa_size;

	// -1 denotes unused; non-negative means the belonging sessfd.
	memset(fd_table, -1, FD_OFFSET);
	
	// Get environment variable indicating the port of the server
	serverport = getenv("serverport15440");
	if (serverport) port = (unsigned short)atoi(serverport);
	else port=15440;
	port = 15226; // For local test
	
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
	
	// main server loop, concurrently handle clients with unlimited requests
	while(1) {
		pid = 100;
		fprintf(stderr, "Waiting for client from server %d\n", pid);
		// wait for next client, get session socket
		sa_size = sizeof(struct sockaddr_in);
		// accept() blocks until a client has connected(, then fork a child process to deal with it)
		sessfd = accept(sockfd, (struct sockaddr *)&cli, &sa_size);
		if (sessfd<0) err(1,0);
		
		pid = fork(); 
		if (pid == 0) {  // child process
			close(sockfd);  // child does not need this

			// get messages and send replies to this client, until it goes away
			while ( (rv=recv(sessfd, buf, MAXMSGLEN, 0)) > 0) { // receive up to MAXMSGLEN bytes into buf
				buf[rv]=0;		// null terminate string to print
				fprintf(stderr, "received msg from server %d: ", pid);
				printf("%s\n", buf);  // print the received messege
				
				msg = malloc(MAXMSGLEN);
				execute_request(buf, msg, &msg_len, sessfd);

				// send reply
				fprintf(stderr, "server replying to client from server %d\n", pid);
				send_rv = send(sessfd, msg, msg_len, 0);
				if (send_rv<0) err(1,0);
			}

			// if received bytes < 0, either client closed connection, or error
			if (rv<0) err(1,0);
			fprintf(stderr, "child server shutting down from server %d\n", pid);
			close(sessfd);  // done with this client
			exit(0);
		}

		// parent process
		close(sessfd);  // parent does not need this
	}
	
	fprintf(stderr, "server shutting down cleanly from server %d\n", pid);
	// close socket
	close(sockfd);  // server is done

	return 0;
}

