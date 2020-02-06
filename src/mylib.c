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
#include <dirtree.h>

#define MAX_PATHNAME 1024
#define FD_OFFSET 2000

int sockfd;
char ack[9] = "received";

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

int fd_server2client(int fd) { return fd + FD_OFFSET; }
int fd_client2server(int fd) { return fd - FD_OFFSET; }

int unpack_tree(char *sub_pkt, struct dirtreenode* sub) {
	fprintf(stderr, "Starting unpacking tree ...\n");
	int i, sub_length = sizeof(struct dirtreenode) + MAX_PATHNAME;

	sub->name = (char *)malloc(MAX_PATHNAME);
	memcpy(sub->name, sub_pkt, MAX_PATHNAME);
	memcpy(&(sub->num_subdirs), sub_pkt + MAX_PATHNAME, sizeof(int));
	fprintf(stderr, "Get subdirs: %d, name: %s\n", sub->num_subdirs, sub->name);

	if (sub->num_subdirs > 0) {
		sub->subdirs = (struct dirtreenode **)malloc(sizeof(struct dirtreenode) * sub->num_subdirs);
		for (i = 0; i < sub->num_subdirs; i++) {
		fprintf(stderr, "Get in subdirs: %d, length: %d\n", i, sub_length);
			sub_length += unpack_tree(sub_pkt + sub_length, sub->subdirs[i]);
		}
	}
	fprintf(stderr, "Safely ended, length: %d\n", sub_length);
	return sub_length;
}

void connect2server() {
	int rv;
	char *serverip;
	char *serverport;
	unsigned short port;
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
	port = 15226; // For local test
	
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
}

void send_pkt(char* pkt, int pkt_len) {
	int rv, sent = 0;

	// first send the pkt length
	rv = send(sockfd, &pkt_len, sizeof(int), 0);
	if (rv < 0) err(1,0);
	fprintf(stderr, "pkt_len: %d	", pkt_len);
	rv = recv(sockfd, ack, strlen(ack) + 1, 0);
	if (rv < 0) err(1,0);
	fprintf(stderr, "ack recved.	");

	// send packet to server
	fprintf(stderr, "client sending to server\n");
	while (sent < pkt_len) {
		rv = send(sockfd, pkt + sent, pkt_len - sent, 0);  // check the rv to make sure the completency
		if (rv < 0) err(1,0);
		sent += rv;
	}
}

void recv_msg(char* buf, int msg_len) {
	int rv, err_no, sent = 0;
	// get packet back
	while ( (rv = recv(sockfd, buf + sent, msg_len, MSG_WAITALL)) > 0) { // receive msg_len bytes into buf
		if (rv < 0) err(1,0);
		sent += rv;
		if (sent >= msg_len)
			break;
	}
	buf[msg_len]=0;				// null terminate string to print
	fprintf(stderr, "client receive messge from the server\n\n");
	
	// Set errno
	memcpy(&err_no, buf, sizeof(int));
	errno = err_no;
}

// Replacement for the open function from libc.
int open(const char *pathname, int flags, ...) {
	int rv, opcode, param_len, msg_len, str_len = (int)strlen(pathname);
	char *pkt, *rt_pkt, *param;
	mode_t m=0;
	if (flags & O_CREAT) {
		va_list a;
		va_start(a, flags);
		m = va_arg(a, mode_t);
		va_end(a);
	}
	fprintf(stderr, "--[OPEN] called for flags: %d, mode: %d, path: %s\n", flags, (int)m, pathname);

	// param packing
	param_len = 2 * sizeof(int) + sizeof(mode_t) + str_len;
	param = malloc(param_len);
    memcpy(param, &flags, sizeof(int));
	memcpy(param + sizeof(int), &m, sizeof(mode_t));
	memcpy(param + sizeof(int) + sizeof(mode_t), &str_len, sizeof(int));
	memcpy(param + 2 * sizeof(int) + sizeof(mode_t), pathname, str_len);

	// pkt packing
	opcode = 1;
	pkt = malloc(sizeof(int) + param_len);
	memcpy(pkt, &opcode, sizeof(int));
	memcpy(pkt + sizeof(int), param, param_len);

	send_pkt(pkt, sizeof(int) + param_len);

	// receive the pkt length
	rv = recv(sockfd, &msg_len, sizeof(int), 0);
	if (rv<0) err(1,0);
	fprintf(stderr, "msg_len: %d	", msg_len);
	rv = send(sockfd, ack, strlen(ack) + 1, 0);
	if (rv<0) err(1,0);

	rt_pkt = (char *)malloc(msg_len + 1);
	recv_msg(rt_pkt, msg_len);

	memcpy(&rv, rt_pkt + sizeof(int), sizeof(int));
	rv = fd_server2client(rv);
	return rv;
}

int close(int fildes) {
	if (fildes < FD_OFFSET) {
		fprintf(stderr, "--[local close] called from %d\n", fildes);
		return orig_close(fildes);
	}
	int rv, opcode, param_len, msg_len;
	char *pkt, *rt_pkt, *param;
	int fd = fd_client2server(fildes);

	fprintf(stderr, "--[rpc close] called from %d\n", fd);

	// param packing
	param_len = sizeof(int);
	param = malloc(param_len);
    memcpy(param, &fd, sizeof(int));

	// pkt packing
	opcode = 2;
	pkt = malloc(sizeof(int) + param_len);
	memcpy(pkt, &opcode, sizeof(int));
	memcpy(pkt + sizeof(int), param, param_len);

	send_pkt(pkt, sizeof(int) + param_len);

	// receive the pkt length
	rv = recv(sockfd, &msg_len, sizeof(int), 0);
	if (rv<0) err(1,0);
	fprintf(stderr, "msg_len: %d	", msg_len);
	rv = send(sockfd, ack, strlen(ack) + 1, 0);
	if (rv<0) err(1,0);

	rt_pkt = (char *)malloc(msg_len + 1);
	recv_msg(rt_pkt, msg_len);

	memcpy(&rv, rt_pkt + sizeof(int), sizeof(int));
	return rv;
}

ssize_t write(int fildes, const void *buf, size_t nbyte) {
	if (fildes < FD_OFFSET) {
		fprintf(stderr, "--[local write] called from %d\n", fildes);
		return orig_write(fildes, buf, nbyte);
	}
	int opcode, param_len, msg_len;
	ssize_t rv;
	char *pkt, *rt_pkt, *param;
	int fd = fd_client2server(fildes);

	fprintf(stderr, "--[rpc write] called from %d, nbyte: %d, buf: %s\n", fd, (int)nbyte, (char *)buf);

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

	send_pkt(pkt, sizeof(int) + param_len);

	// receive the pkt length
	rv = recv(sockfd, &msg_len, sizeof(int), 0);
	if (rv<0) err(1,0);
	fprintf(stderr, "msg_len: %d	", msg_len);
	rv = send(sockfd, ack, strlen(ack) + 1, 0);
	if (rv<0) err(1,0);

	rt_pkt = (char *)malloc(msg_len + 1);
	recv_msg(rt_pkt, msg_len);

	memcpy(&rv, rt_pkt + sizeof(int), sizeof(int));
	return rv;
}

ssize_t read(int fildes, void *buf, size_t nbyte) {
	if (fildes < FD_OFFSET) {
		fprintf(stderr, "--[local read] called from %d\n", fildes);
		return orig_read(fildes, buf, nbyte);
	}
	int opcode, param_len, msg_len;
	ssize_t rv;
	char *pkt, *rt_pkt, *param;
	int fd = fd_client2server(fildes);

	fprintf(stderr, "--[rpc read] called from %d, nbyte: %d, buf: %s\n", fd, (int)nbyte, (char *)buf);

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

	send_pkt(pkt, sizeof(int) + param_len);

	// receive the pkt length
	rv = recv(sockfd, &msg_len, sizeof(int), 0);
	if (rv<0) err(1,0);
	fprintf(stderr, "msg_len: %d	", msg_len);
	rv = send(sockfd, ack, strlen(ack) + 1, 0);
	if (rv<0) err(1,0);

	rt_pkt = (char *)malloc(msg_len + 1);
	recv_msg(rt_pkt, msg_len);

	// pkt unpacking
	memcpy(&rv, rt_pkt + sizeof(int), sizeof(int));
	memcpy(buf, rt_pkt + 2 * sizeof(int), nbyte);
	fprintf(stderr, "mylib: read called ended, buf: %s\n", (char *)buf);
	return rv;
}

off_t lseek(int fildes, off_t offset, int whence) {
	if (fildes < FD_OFFSET) {
		fprintf(stderr, "--[local] lseek called from %d\n", fildes);
		return orig_lseek(fildes, offset, whence);
	}
	int opcode, param_len, msg_len;
	off_t rv;
	char *pkt, *rt_pkt, *param;
	int fd = fd_client2server(fildes);

	fprintf(stderr, "--[rpc lseek] called from %d\n", fd);

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

	send_pkt(pkt, sizeof(int) + param_len);

	// receive the pkt length
	rv = recv(sockfd, &msg_len, sizeof(int), 0);
	if (rv<0) err(1,0);
	fprintf(stderr, "msg_len: %d	", msg_len);
	rv = send(sockfd, ack, strlen(ack) + 1, 0);
	if (rv<0) err(1,0);

	rt_pkt = (char *)malloc(msg_len + 1);
	recv_msg(rt_pkt, msg_len);

	// pkt unpacking
	memcpy(&rv, rt_pkt + sizeof(int), sizeof(int));
	return rv;
}

int stat(const char *pathname, struct stat *buf) {
	int rv, opcode, param_len, msg_len, str_len = (int)strlen(pathname);
	char *pkt, *rt_pkt, *param;

	fprintf(stderr, "--[stat] called for path %s\n", pathname);

	// param packing
	param_len = sizeof(int) + str_len;
	param = malloc(param_len);
    memcpy(param, &str_len, sizeof(int));
    memcpy(param + sizeof(int), pathname, str_len);

	// pkt packing
	opcode = 6;
	pkt = malloc(sizeof(int) + param_len);
	memcpy(pkt, &opcode, sizeof(int));
	memcpy(pkt + sizeof(int), param, param_len);

	send_pkt(pkt, sizeof(int) + param_len);

	// receive the pkt length
	rv = recv(sockfd, &msg_len, sizeof(int), 0);
	if (rv<0) err(1,0);
	fprintf(stderr, "msg_len: %d	", msg_len);
	rv = send(sockfd, ack, strlen(ack) + 1, 0);
	if (rv<0) err(1,0);

	rt_pkt = (char *)malloc(msg_len + 1);
	recv_msg(rt_pkt, msg_len);

	// pkt unpacking
	memcpy(&rv, rt_pkt + sizeof(int), sizeof(int));
	memcpy(buf, rt_pkt + 2 * sizeof(int), sizeof(struct stat));
	return rv;
}

int __xstat(int ver, const char * pathname, struct stat * stat_buf) {
	int rv, opcode, param_len, msg_len, str_len = (int)strlen(pathname);
	char *pkt, *rt_pkt, *param;

	fprintf(stderr, "--[__xstat] called for path %s\n", pathname);

	// param packing
	param_len = 2 * sizeof(int) + str_len;
	param = malloc(param_len);
	memcpy(param, &ver, sizeof(int));
	memcpy(param + sizeof(int), &str_len, sizeof(int));
	memcpy(param + 2 * sizeof(int), pathname, str_len);

	// pkt packing
	opcode = 7;
	pkt = malloc(sizeof(int) + param_len);
	memcpy(pkt, &opcode, sizeof(int));
	memcpy(pkt + sizeof(int), param, param_len);

	send_pkt(pkt, sizeof(int) + param_len);

	// receive the pkt length
	rv = recv(sockfd, &msg_len, sizeof(int), 0);
	if (rv<0) err(1,0);
	fprintf(stderr, "msg_len: %d	", msg_len);
	rv = send(sockfd, ack, strlen(ack) + 1, 0);
	if (rv<0) err(1,0);

	rt_pkt = (char *)malloc(msg_len + 1);
	recv_msg(rt_pkt, msg_len);

	// pkt unpacking
	memcpy(&rv, rt_pkt + sizeof(int), sizeof(int));
	memcpy(stat_buf, rt_pkt + 2 * sizeof(int), sizeof(struct stat));
	return rv;
}

int unlink(const char *pathname) {
	int rv, opcode, param_len, msg_len, str_len = (int)strlen(pathname);
	char *pkt, *rt_pkt, *param;

	fprintf(stderr, "--[unlink] called for path %s\n", pathname);

	// param packing
	param_len = sizeof(int) + str_len;
	param = malloc(param_len);
    memcpy(param, &str_len, sizeof(int));
    memcpy(param + sizeof(int), pathname, str_len);

	// pkt packing
	opcode = 8;
	pkt = malloc(sizeof(int) + param_len);
	memcpy(pkt, &opcode, sizeof(int));
	memcpy(pkt + sizeof(int), param, param_len);

	send_pkt(pkt, sizeof(int) + param_len);

	// receive the pkt length
	rv = recv(sockfd, &msg_len, sizeof(int), 0);
	if (rv<0) err(1,0);
	fprintf(stderr, "msg_len: %d	", msg_len);
	rv = send(sockfd, ack, strlen(ack) + 1, 0);
	if (rv<0) err(1,0);

	rt_pkt = (char *)malloc(msg_len + 1);
	recv_msg(rt_pkt, msg_len);

	memcpy(&rv, rt_pkt + sizeof(int), sizeof(int));
	return rv;
}

int getdirentries(int fd, char *buf, int nbytes, long *basep) {
	if (fd < FD_OFFSET) {
		fprintf(stderr, "--[local] getdirentries called from %d\n", fd);
		return orig_getdirentries(fd, buf, nbytes, basep);
	}
	int rv, opcode, param_len, msg_len;
	char *pkt, *rt_pkt, *param;
	int fid = fd_client2server(fd);

	fprintf(stderr, "--[rpc getdirentries] called from %d, nbytes: %d\n", fid, nbytes);

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

	send_pkt(pkt, sizeof(int) + param_len);

	// receive the pkt length
	rv = recv(sockfd, &msg_len, sizeof(int), 0);
	if (rv<0) err(1,0);
	fprintf(stderr, "msg_len: %d	", msg_len);
	rv = send(sockfd, ack, strlen(ack) + 1, 0);
	if (rv<0) err(1,0);

	rt_pkt = (char *)malloc(msg_len + 1);
	recv_msg(rt_pkt, msg_len);

	// pkt unpacking
	memcpy(&rv, rt_pkt + sizeof(int), sizeof(int));
	memcpy(buf, rt_pkt + 2 * sizeof(int), nbytes);
	return rv;
}

struct dirtreenode* getdirtree(const char *pathname) {
	int opcode, param_len, msg_len, recv_rv, length, rt_length, str_len = (int)strlen(pathname);
	char *pkt, *rt_pkt, *param;
	struct dirtreenode* rv = (struct dirtreenode *)malloc(sizeof(struct dirtreenode));

	fprintf(stderr, "--[getdirtree] called for path %s\n", pathname);

	// param packing
	param_len = sizeof(int) + str_len;
	param = malloc(param_len);
    memcpy(param, &str_len, sizeof(int));
    memcpy(param + sizeof(int), pathname, str_len);

	// pkt packing
	opcode = 10;
	pkt = malloc(sizeof(int) + param_len);
	memcpy(pkt, &opcode, sizeof(int));
	memcpy(pkt + sizeof(int), param, param_len);

	send_pkt(pkt, sizeof(int) + param_len);

	// receive the pkt length
	recv_rv = recv(sockfd, &msg_len, sizeof(int), 0);
	if (recv_rv<0) err(1,0);
	fprintf(stderr, "msg_len: %d	", msg_len);
	recv_rv = send(sockfd, ack, strlen(ack) + 1, 0);
	if (recv_rv<0) err(1,0);

	rt_pkt = (char *)malloc(msg_len + 1);
	recv_msg(rt_pkt, msg_len);

	memcpy(&rt_length, rt_pkt + sizeof(int), sizeof(int));
	fprintf(stderr, "mylib: getdirtree called ended, rt_length: %d\n", rt_length);
	length = unpack_tree(rt_pkt + 2 * sizeof(int), rv);
	fprintf(stderr, "length: %d\n", length);
	return rv;
}

// this func actually doesn't need to be an RPC
void freedirtree(struct dirtreenode* dt) {
	fprintf(stderr, "--[freedirtree] called\n");
	// just free it locally
	orig_freedirtree(dt); // may need to write a free() by yourself
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

	connect2server();
	fprintf(stderr, "Init mylib: connected to server\n");
}

// Automatically called when program is ended
void _fini(void) {
	// close socket
	orig_close(sockfd);  // client is done
}
