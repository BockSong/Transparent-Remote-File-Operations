#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

// The following line declares a function pointer with the same prototype as the open function.  
int (*orig_open)(const char *pathname, int flags, ...);  // mode_t mode is needed when flags includes O_CREAT
int (*orig_close)(int fildes);
ssize_t (*orig_read)(int fildes, void *buf, size_t nbyte);
ssize_t (*orig_write)(int fildes, const void *buf, size_t nbyte);
off_t (*orig_lseek)(int fildes, off_t offset, int whence);
int (*orig_stat)(const char *pathname, struct stat *buf);
int (*orig_unlink)(const char *pathname);
int (*orig_getdirentries)(int fd, char *buf, int nbytes, long *basep);
struct dirtreenode* (*orig_getdirtree)(const char *pathname);
void (*orig_freedirtree)(struct dirtreenode* dt);

// This is our replacement for the open function from libc.
int open(const char *pathname, int flags, ...) {
	mode_t m=0;
	if (flags & O_CREAT) {
		va_list a;
		va_start(a, flags);
		m = va_arg(a, mode_t);
		va_end(a);
	}
	// we just print a message, then call through to the original open function (from libc)
	fprintf(stderr, "mylib: open called for path %s\n", pathname);
	return orig_open(pathname, flags, m);
}

int close(int fildes) {
	fprintf(stderr, "mylib: close called from %d\n", fildes);
	return orig_close(fildes);
}

ssize_t read(int fildes, void *buf, size_t nbyte) {
	fprintf(stderr, "mylib: read called from %d\n", fildes);
	return orig_read(fildes, buf, nbyte);
}

ssize_t write(int fildes, const void *buf, size_t nbyte) {
	fprintf(stderr, "mylib: write called from %d\n", fildes);
	return orig_write(fildes, buf, nbyte);
}

off_t lseek(int fildes, off_t offset, int whence) {
	fprintf(stderr, "mylib: lseek called from %d\n", fildes);
	return orig_lseek(fildes, offset, whence);
}

int stat(const char *pathname, struct stat *buf) {
	fprintf(stderr, "mylib: stat called for path %s\n", pathname);
	return orig_stat(pathname, buf);
}

int unlink(const char *pathname) {
	fprintf(stderr, "mylib: unlink called for path %s\n", pathname);
	return orig_unlink(pathname);
}

int getdirentries(int fd, char *buf, int nbytes, long *basep) {
	fprintf(stderr, "mylib: getdirentries called from %d\n", fd);
	return orig_getdirentries(fd, buf, nbytes, basep);
}

struct dirtreenode* getdirtree(const char *pathname) {
	fprintf(stderr, "mylib: getdirtree called for path %s\n", pathname);
	return orig_getdirtree(pathname);
}

void freedirtree(struct dirtreenode* dt) {
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
	orig_unlink = dlsym(RTLD_NEXT, "unlink");
	orig_getdirentries = dlsym(RTLD_NEXT, "getdirentries");
	orig_getdirtree = dlsym(RTLD_NEXT, "getdirtree");
	orig_freedirtree = dlsym(RTLD_NEXT, "freedirtree");
	fprintf(stderr, "Init mylib\n");
}


