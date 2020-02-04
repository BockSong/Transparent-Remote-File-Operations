#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <err.h>
#include <errno.h>

#define MAXMSGLEN 4096
#define MAX_PATHNAME 256
#define FD_OFFSET 2000

// The following line declares function pointers with the same prototype as the original functions.  
int (*orig_open)(const char *pathname, int flags, ...);  // mode_t mode is needed when flags includes O_CREAT
int (*orig_close)(int fildes);
ssize_t (*orig_read)(int fildes, void *buf, size_t nbyte);
ssize_t (*orig_write)(int fildes, const void *buf, size_t nbyte);
off_t (*orig_lseek)(int fildes, off_t offset, int whence);
int (*orig_stat)(const char *pathname, struct stat *buf);
int (*orig___xstat)(int ver, const char * pathname, struct stat * stat_buf);
int (*orig_unlink)(const char *pathname);
int (*orig_getdirentries)(int fd, char *buf, int nbytes, long *basep);
struct dirtreenode* (*orig_getdirtree)(const char *pathname);
void (*orig_freedirtree)(struct dirtreenode* dt);

int fd_server2client(int fd) { return fd + 1024; }
int fd_client2server(int fd) { return fd - 1024; }

int connect2server() {
	char *serverip;
	char *serverport;
	unsigned short port;
	int sockfd, rv;
	struct sockaddr_in srv;     // address structure
	
	// Get environment variable indicating the ip address of the server
	serverip = getenv("server15440");
	if (serverip) fprintf(stderr, "Got environment variable server15440: %s\n", serverip);
	else {
		fprintf(stderr, "Environment variable server15440 not found.  Using 127.0.0.1\n");
		serverip = "127.0.0.1";
	}
	
	// Get environment variable indicating the port of the server
	serverport = getenv("serverport15440");
	if (serverport) fprintf(stderr, "Got environment variable serverport15440: %s\n", serverport);
	else {
		fprintf(stderr, "Environment variable serverport15440 not found.  Using 15440\n");
		serverport = "15440";
	}
	port = (unsigned short)atoi(serverport);
	port = 15228; // For local test
	
	// Create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);	// TCP/IP socket
	if (sockfd<0) err(1, 0);			// in case of error
	
	// setup address structure to point to server
	memset(&srv, 0, sizeof(srv));			// clear it first
	srv.sin_family = AF_INET;			// IP family
	srv.sin_addr.s_addr = inet_addr(serverip);	// IP address of server
	srv.sin_port = htons(port);			// server port

	// actually connect to the server
	rv = connect(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
	if (rv<0) err(1,0);

	return sockfd;
}

void contact2server(int sockfd, char* pkt, int pkt_len, char* buf) {
	int rv, err_no;

	// send packet to server
	fprintf(stderr, "client sending to server\n");
	rv = send(sockfd, pkt, pkt_len, 0);	// send the whole packet
	if (rv<0) err(1,0);
	
	// get packet back
	rv = recv(sockfd, buf, MAXMSGLEN, 0);	// receive upto MAXMSGLEN bytes into buf
	if (rv<0) err(1,0);			// in case something went wrong
	buf[rv]=0;				// null terminate string to print
	fprintf(stderr, "client got messge (Not string format)\n");
	
	// close socket
	orig_close(sockfd);  // client is done

	// Set errno
	memcpy(&err_no, buf, sizeof(int));
	errno = err_no;
}

// To be removed
void contact2server_local(int sockfd, char *msg) {
	char buf[MAXMSGLEN+1];
	int rv;

	// send message to server
	fprintf(stderr, "client sending to server: %s\n", msg);
	rv = send(sockfd, msg, strlen(msg), 0);	// send the whole msg
	if (rv<0) err(1,0);
	
	// get message back
	rv = recv(sockfd, buf, MAXMSGLEN, 0);	// receive upto MAXMSGLEN bytes into buf
	if (rv<0) err(1,0);			// in case something went wrong
	buf[rv]=0;				// null terminate string to print
	fprintf(stderr, "client got messge: %s\n", buf);
	
	// close socket
	orig_close(sockfd);  // client is done
}

// Replacement for the open function from libc.
int open(const char *pathname, int flags, ...) {
	int sockfd, rv, param_len, opcode;
	char *pkt, rt_pkt[MAXMSGLEN+1], *param;
	mode_t m=0;
	if (flags & O_CREAT) {
		va_list a;
		va_start(a, flags);
		m = va_arg(a, mode_t);
		va_end(a);
	}

	fprintf(stderr, "mylib: open called for flags: %d, mode: %d, path: %s\n", flags, (int)m, pathname);
	sockfd = connect2server();

	// param packing
	param_len = sizeof(int) + sizeof(mode_t) + MAX_PATHNAME;
	param = malloc(param_len);
    memcpy(param, &flags, sizeof(int));
	memcpy(param + sizeof(int), &m, sizeof(mode_t));
	memcpy(param + sizeof(int) + sizeof(mode_t), pathname, MAX_PATHNAME);

	// pkt packing
	opcode = 1;
	pkt = malloc(sizeof(int) + param_len);
	memcpy(pkt, &opcode, sizeof(int));
	memcpy(pkt + sizeof(int), param, param_len);

	contact2server(sockfd, pkt, sizeof(int) + param_len, rt_pkt);
	
	memcpy(&rv, rt_pkt + sizeof(int), sizeof(int));
	rv = fd_server2client(rv);
	return rv;
}

int close(int fildes) {
	int sockfd, rv, param_len, opcode;
	char *pkt, rt_pkt[MAXMSGLEN+1], *param;
	int fd = fd_client2server(fildes);

	fprintf(stderr, "mylib: close called from %d\n", fd);
	sockfd = connect2server();

	// param packing
	param_len = sizeof(int);
	param = malloc(param_len);
    memcpy(param, &fd, sizeof(int));

	// pkt packing
	opcode = 2;
	pkt = malloc(sizeof(int) + param_len);
	memcpy(pkt, &opcode, sizeof(int));
	memcpy(pkt + sizeof(int), param, param_len);

	contact2server(sockfd, pkt, sizeof(int) + param_len, rt_pkt);
	
	memcpy(&rv, rt_pkt + sizeof(int), sizeof(int));
	return rv;
}

ssize_t write(int fildes, const void *buf, size_t nbyte) {
	int sockfd, param_len, opcode;
	ssize_t rv;
	char *pkt, rt_pkt[MAXMSGLEN+1], *param;
	int fd = fd_client2server(fildes);

	fprintf(stderr, "mylib: write called from %d, nbyte: %d, buf: %s\n", fd, (int)nbyte, (char *)buf);
	sockfd = connect2server();

	// param packing
	param_len = sizeof(int) + sizeof(size_t) + nbyte;
	param = malloc(param_len);
	memcpy(param, &fd, sizeof(int));
	memcpy(param + sizeof(int), &nbyte, sizeof(size_t));
	memcpy(param + sizeof(int) + sizeof(size_t), buf, nbyte);
	
	// pkt packing
	opcode = 3;
	pkt = malloc(sizeof(int) + param_len);
	memcpy(pkt, &opcode, sizeof(int));
	memcpy(pkt + sizeof(int), param, param_len);

	contact2server(sockfd, pkt, sizeof(int) + param_len, rt_pkt);
	
	memcpy(&rv, rt_pkt + sizeof(int), sizeof(int));
	return rv;
}

ssize_t read(int fildes, void *buf, size_t nbyte) {
	int sockfd, param_len, opcode;
	ssize_t rv;
	char *pkt, rt_pkt[MAXMSGLEN+1], *param;
	int fd = fd_client2server(fildes);

	fprintf(stderr, "mylib: read called from %d, nbyte: %d, buf: %s\n", fd, (int)nbyte, (char *)buf);
	sockfd = connect2server();

	// param packing
	param_len = sizeof(int) + sizeof(size_t);
	param = malloc(param_len);
	memcpy(param, &fd, sizeof(int));
	memcpy(param + sizeof(int), &nbyte, sizeof(size_t));
	
	// pkt packing
	opcode = 4;
	pkt = malloc(sizeof(int) + param_len);
	memcpy(pkt, &opcode, sizeof(int));
	memcpy(pkt + sizeof(int), param, param_len);

	contact2server(sockfd, pkt, sizeof(int) + param_len, rt_pkt);
	
	// pkt unpacking
	memcpy(&rv, rt_pkt + sizeof(int), sizeof(int));
	memcpy(buf, rt_pkt + 2 * sizeof(int), nbyte);
	fprintf(stderr, "mylib: read called ended, buf: %s\n", (char *)buf);
	return rv;
}

off_t lseek(int fildes, off_t offset, int whence) {
	int sockfd, param_len, opcode;
	off_t rv;
	char *pkt, rt_pkt[MAXMSGLEN+1], *param;
	int fd = fd_client2server(fildes);

	fprintf(stderr, "mylib: lseek called from %d\n", fd);
	sockfd = connect2server();

	// param packing
	param_len = 2 * sizeof(int) + sizeof(off_t);
	param = malloc(param_len);
	memcpy(param, &fd, sizeof(int));
	memcpy(param + sizeof(int), &offset, sizeof(off_t));
	memcpy(param + sizeof(int) + sizeof(off_t), &whence, sizeof(int));
	
	// pkt packing
	opcode = 5;
	pkt = malloc(sizeof(int) + param_len);
	memcpy(pkt, &opcode, sizeof(int));
	memcpy(pkt + sizeof(int), param, param_len);

	contact2server(sockfd, pkt, sizeof(int) + param_len, rt_pkt);
	
	// pkt unpacking
	memcpy(&rv, rt_pkt + sizeof(int), sizeof(int));
	return rv;
}

// TODO: do FD transformation?
int stat(const char *pathname, struct stat *buf) {
	int sockfd, param_len, opcode, rv;
	char *pkt, rt_pkt[MAXMSGLEN+1], *param;

	fprintf(stderr, "mylib: stat called for path %s\n", pathname);
	sockfd = connect2server();

	// param packing
	param_len = MAX_PATHNAME;
	param = malloc(param_len);
    memcpy(param, pathname, MAX_PATHNAME);

	// pkt packing
	opcode = 6;
	pkt = malloc(sizeof(int) + param_len);
	memcpy(pkt, &opcode, sizeof(int));
	memcpy(pkt + sizeof(int), param, param_len);

	contact2server(sockfd, pkt, sizeof(int) + param_len, rt_pkt);
	
	// pkt unpacking
	memcpy(&rv, rt_pkt + sizeof(int), sizeof(int));
	memcpy(buf, rt_pkt + 2 * sizeof(int), sizeof(struct stat));
	return rv;
}

// TODO: do FD transformation?
int __xstat(int ver, const char * pathname, struct stat * stat_buf) {
	int sockfd, param_len, opcode, rv;
	char *pkt, rt_pkt[MAXMSGLEN+1], *param;

	fprintf(stderr, "mylib: __xstat called for path %s\n", pathname);
	sockfd = connect2server();

	// param packing
	param_len = sizeof(int) + MAX_PATHNAME;
	param = malloc(param_len);
	memcpy(param, &ver, sizeof(int));
	memcpy(param + sizeof(int), pathname, MAX_PATHNAME);

	// pkt packing
	opcode = 7;
	pkt = malloc(sizeof(int) + param_len);
	memcpy(pkt, &opcode, sizeof(int));
	memcpy(pkt + sizeof(int), param, param_len);

	contact2server(sockfd, pkt, sizeof(int) + param_len, rt_pkt);
	
	// pkt unpacking
	memcpy(&rv, rt_pkt + sizeof(int), sizeof(int));
	memcpy(stat_buf, rt_pkt + 2 * sizeof(int), sizeof(struct stat));
	return rv;
}

int unlink(const char *pathname) {
	int sockfd, param_len, opcode, rv;
	char *pkt, rt_pkt[MAXMSGLEN+1], *param;

	fprintf(stderr, "mylib: unlink called for path %s\n", pathname);
	sockfd = connect2server();

	// param packing
	param_len = MAX_PATHNAME;
	param = malloc(param_len);
    memcpy(param, pathname, MAX_PATHNAME);

	// pkt packing
	opcode = 8;
	pkt = malloc(sizeof(int) + param_len);
	memcpy(pkt, &opcode, sizeof(int));
	memcpy(pkt + sizeof(int), param, param_len);

	contact2server(sockfd, pkt, sizeof(int) + param_len, rt_pkt);
	
	memcpy(&rv, rt_pkt + sizeof(int), sizeof(int));
	return rv;
}

int getdirentries(int fd, char *buf, int nbytes, long *basep) {
	int sockfd, param_len, opcode;
	int rv;
	char *pkt, rt_pkt[MAXMSGLEN+1], *param;
	int fid = fd_client2server(fd);

	fprintf(stderr, "mylib: getdirentries called from %d, nbytes: %d\n", fid, nbytes);
	sockfd = connect2server();

	// param packing
	param_len = 2 * sizeof(int) + sizeof(long);
	param = malloc(param_len);
	memcpy(param, &fid, sizeof(int));
	memcpy(param + sizeof(int), &nbytes, sizeof(int));
	memcpy(param + 2 * sizeof(int), basep, sizeof(long));
	
	// pkt packing
	opcode = 9;
	pkt = malloc(sizeof(int) + param_len);
	memcpy(pkt, &opcode, sizeof(int));
	memcpy(pkt + sizeof(int), param, param_len);

	contact2server(sockfd, pkt, sizeof(int) + param_len, rt_pkt);
	
	// pkt unpacking
	memcpy(&rv, rt_pkt + sizeof(int), sizeof(int));
	memcpy(buf, rt_pkt + 2 * sizeof(int), nbytes);
	fprintf(stderr, "getdirentries: read called ended, buf: %s\n", (char *)buf);
	return rv;
}

struct dirtreenode* getdirtree(const char *pathname) {
	int sockfd;

	fprintf(stderr, "mylib: getdirtree called for path %s\n", pathname);
	sockfd = connect2server();
	contact2server_local(sockfd, "getdirtree");
	return orig_getdirtree(pathname);
}

// Hint: does this func really need to be an RPC?
void freedirtree(struct dirtreenode* dt) {
	int sockfd;
	fprintf(stderr, "mylib: freedirtree called\n");
	sockfd = connect2server();
	contact2server_local(sockfd, "freedirtree");
	return orig_freedirtree(dt);
}

// This function is automatically called when program is started
void _init(void) {
	// set function pointers to point to the original function calls
	orig_open = dlsym(RTLD_NEXT, "open");
	orig_close = dlsym(RTLD_NEXT, "close");
	orig_read = dlsym(RTLD_NEXT, "read");
	orig_write = dlsym(RTLD_NEXT, "write");
	orig_lseek = dlsym(RTLD_NEXT, "lseek");
	orig_stat = dlsym(RTLD_NEXT, "stat");
	orig___xstat = dlsym(RTLD_NEXT, "__xstat");
	orig_unlink = dlsym(RTLD_NEXT, "unlink");
	orig_getdirentries = dlsym(RTLD_NEXT, "getdirentries");
	orig_getdirtree = dlsym(RTLD_NEXT, "getdirtree");
	orig_freedirtree = dlsym(RTLD_NEXT, "freedirtree");
	fprintf(stderr, "Init mylib\n");
}

