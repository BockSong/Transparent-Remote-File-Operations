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
	//port = 15228; // For local test
	
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

//void contact2server(int sockfd, packet* pkt, packet* rt_pkt) {
void contact2server(int sockfd, char* pkt, int pkt_len, char* buf) {
	int rv;

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
}

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

// This is our replacement for the open function from libc.
int open(const char *pathname, int flags, ...) {
	int sockfd, rv, err_no;
	char *pkt, rt_pkt[MAXMSGLEN+1];
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
	int param_len = sizeof(int) + sizeof(mode_t) + MAX_PATHNAME;
	char *param = malloc(param_len);
    memcpy(param, &flags, sizeof(int));
	memcpy(param + sizeof(int), &m, sizeof(mode_t));
	memcpy(param + sizeof(int) + sizeof(mode_t), pathname, MAX_PATHNAME);

	// pkt packing
	int opcode = 1;
	pkt = malloc(sizeof(int) + param_len);
	memcpy(pkt, &opcode, sizeof(int));
	memcpy(pkt + sizeof(int), param, param_len);

	contact2server(sockfd, pkt, sizeof(int) + param_len, rt_pkt);
	
	memcpy(&rv, rt_pkt, sizeof(int));
	memcpy(&err_no, rt_pkt + sizeof(int), sizeof(int));
	errno = err_no;

	fprintf(stderr, "mylib: open call ended\n");
	return rv;
}

int close(int fildes) {
	int sockfd, rv, err_no;
	char *pkt, rt_pkt[MAXMSGLEN+1];

	fprintf(stderr, "mylib: close called from %d\n", fildes);
	sockfd = connect2server();

	// param packing
	int param_len = sizeof(int);
	char *param = malloc(param_len);
    memcpy(param, &fildes, sizeof(int));

	// pkt packing
	int opcode = 2;
	pkt = malloc(sizeof(int) + param_len);
	memcpy(pkt, &opcode, sizeof(int));
	memcpy(pkt + sizeof(int), param, param_len);

	contact2server(sockfd, pkt, sizeof(int) + param_len, rt_pkt);
	
	memcpy(&rv, rt_pkt, sizeof(int));
	memcpy(&err_no, rt_pkt + sizeof(int), sizeof(int));
	errno = err_no;

	return rv;
}

ssize_t write(int fildes, const void *buf, size_t nbyte) {
	int sockfd, err_no;
	ssize_t rv;
	char *pkt, rt_pkt[MAXMSGLEN+1];

	fprintf(stderr, "mylib: write called from %d\n", fildes);
	sockfd = connect2server();

	// param packing
	int param_len = sizeof(int) + sizeof(size_t) + nbyte;
	char *param = malloc(param_len);
	memcpy(param, &fildes, sizeof(int));
	memcpy(param + sizeof(int), &nbyte, sizeof(size_t));
	memcpy(param + sizeof(int) + sizeof(size_t), buf, nbyte);
	
	// pkt packing
	int opcode = 3;
	pkt = malloc(sizeof(int) + param_len);
	memcpy(pkt, &opcode, sizeof(int));
	memcpy(pkt + sizeof(int), param, param_len);

	// try unpacking
	ssize_t test_nbyte;
	char *test_buf = (char *)malloc(MAXMSGLEN);
	memcpy(&test_nbyte, pkt + 2 * sizeof(int), sizeof(size_t));
	memcpy(test_buf, pkt + 2 * sizeof(int) + sizeof(size_t), nbyte);
	fprintf(stderr, "test_nbyte: %d, test_buf: %s", (int)test_nbyte, test_buf);

	contact2server(sockfd, pkt, sizeof(int) + param_len, rt_pkt);
	
	memcpy(&rv, rt_pkt, sizeof(int));
	memcpy(&err_no, rt_pkt + sizeof(int), sizeof(int));
	errno = err_no;

	return rv;
}

ssize_t read(int fildes, void *buf, size_t nbyte) {
	int sockfd;
	fprintf(stderr, "mylib: read called from %d\n", fildes);
	sockfd = connect2server();
	contact2server_local(sockfd, "read");
	return orig_read(fildes, buf, nbyte);
}

off_t lseek(int fildes, off_t offset, int whence) {
	int sockfd;
	fprintf(stderr, "mylib: lseek called from %d\n", fildes);
	sockfd = connect2server();
	contact2server_local(sockfd, "lseek");
	return orig_lseek(fildes, offset, whence);
}

int stat(const char *pathname, struct stat *buf) {
	int sockfd;
	fprintf(stderr, "mylib: stat called for path %s\n", pathname);
	sockfd = connect2server();
	contact2server_local(sockfd, "stat");
	return orig_stat(pathname, buf);
}

int __xstat(int ver, const char * pathname, struct stat * stat_buf) {
	int sockfd;
	fprintf(stderr, "mylib: __xstat called for path %s\n", pathname);
	sockfd = connect2server();
	contact2server_local(sockfd, "stat");
	return orig___xstat(ver, pathname, stat_buf);
}

int unlink(const char *pathname) {
	int sockfd;
	fprintf(stderr, "mylib: unlink called for path %s\n", pathname);
	sockfd = connect2server();
	contact2server_local(sockfd, "unlink");
	return orig_unlink(pathname);
}

int getdirentries(int fd, char *buf, int nbytes, long *basep) {
	int sockfd;
	fprintf(stderr, "mylib: getdirentries called from %d\n", fd);
	sockfd = connect2server();
	contact2server_local(sockfd, "getdirentries");
	return orig_getdirentries(fd, buf, nbytes, basep);
}

struct dirtreenode* getdirtree(const char *pathname) {
	int sockfd;
	fprintf(stderr, "mylib: getdirtree called for path %s\n", pathname);
	sockfd = connect2server();
	contact2server_local(sockfd, "getdirtree");
	return orig_getdirtree(pathname);
}

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

