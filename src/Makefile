# Implicit rules
ROGS=client server
CFLAGS+=-Wall

# Main structure
all: mylib.so server $(PROGS)

mylib.o: mylib.c
	gcc -Wall -fPIC -DPIC -c mylib.c -I../include

mylib.so: mylib.o
	ld -shared -o mylib.so mylib.o -ldl -L../lib

server: server.c
	gcc -Wall -o server server.c

clean:
	rm -rf *.o *.so server $(PROGS)

