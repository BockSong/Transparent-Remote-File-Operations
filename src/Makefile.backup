CC=gcc
CFLAGS=-Wall -Werror -fPIC -DPIC -std=gnu99 -I../include
LDFLAGS=-L../lib
LDLIBS=-ldirtree

all: server mylib.so

server: server.o packet.o

mylib.so: mylib.o packet.o
	$(LD) -shared -o mylib.so mylib.o packet.o $(LDFLAGS) -ldl $(LDLIBS)

.PHONY: all clean
clean:
	rm -rf *.o *.so server
