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

#define MAXMSGLEN 100

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

void connect2server(char *msg) {
	char *serverip;
	char *serverport;
	unsigned short port;
	char buf[MAXMSGLEN+1];
	int sockfd, rv;
	struct sockaddr_in srv;
	
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
	//port = 15332;
	
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
	
	// send message to server
	fprintf(stderr, "client sending to server: %s\n", msg);
	send(sockfd, msg, strlen(msg), 0);	// send message; should check return value
	
	// get message back
	rv = recv(sockfd, buf, MAXMSGLEN, 0);	// get message
	if (rv<0) err(1,0);			// in case something went wrong
	buf[rv]=0;				// null terminate string to print
	fprintf(stderr, "client got messge: %s\n", buf);
	
	// close socket
	orig_close(sockfd);
}

// This is our replacement for the open function from libc.
int open(const char *pathname, int flags, ...) {
	mode_t m=0;
	if (flags & O_CREAT) {
		va_list a;
		va_start(a, flags);
		m = va_arg(a, mode_t);
		va_end(a);
	}

	connect2server("open");
	// we just print a message, then call through to the original open function (from libc)
	fprintf(stderr, "mylib: open called for path %s\n", pathname);
	return orig_open(pathname, flags, m);
}

int close(int fildes) {
	connect2server("close");
	fprintf(stderr, "mylib: close called from %d\n", fildes);
	return orig_close(fildes);
}

ssize_t read(int fildes, void *buf, size_t nbyte) {
	connect2server("read");
	fprintf(stderr, "mylib: read called from %d\n", fildes);
	return orig_read(fildes, buf, nbyte);
}

ssize_t write(int fildes, const void *buf, size_t nbyte) {
	connect2server("write");
	fprintf(stderr, "mylib: write called from %d\n", fildes);
	return orig_write(fildes, buf, nbyte);
}

off_t lseek(int fildes, off_t offset, int whence) {
	connect2server("lseek");
	fprintf(stderr, "mylib: lseek called from %d\n", fildes);
	return orig_lseek(fildes, offset, whence);
}

int stat(const char *pathname, struct stat *buf) {
	connect2server("stat");
	fprintf(stderr, "mylib: stat called for path %s\n", pathname);
	return orig_stat(pathname, buf);
}

int __xstat(int ver, const char * pathname, struct stat * stat_buf) {
	connect2server("stat");
	fprintf(stderr, "mylib: __xstat called for path %s\n", pathname);
	return orig___xstat(ver, pathname, stat_buf);
}

int unlink(const char *pathname) {
	connect2server("unlink");
	fprintf(stderr, "mylib: unlink called for path %s\n", pathname);
	return orig_unlink(pathname);
}

int getdirentries(int fd, char *buf, int nbytes, long *basep) {
	connect2server("getdirentries");
	fprintf(stderr, "mylib: getdirentries called from %d\n", fd);
	return orig_getdirentries(fd, buf, nbytes, basep);
}

struct dirtreenode* getdirtree(const char *pathname) {
	connect2server("getdirtree");
	fprintf(stderr, "mylib: getdirtree called for path %s\n", pathname);
	return orig_getdirtree(pathname);
}

void freedirtree(struct dirtreenode* dt) {
	connect2server("freedirtree");
	fprintf(stderr, "mylib: freedirtree called\n");
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

