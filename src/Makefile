ROGS=client server
CFLAGS+=-Wall

all: mylib.so

mylib.o: mylib.c
	gcc -Wall -fPIC -DPIC -c mylib.c -I../include

mylib.so: mylib.o
	ld -shared -o mylib.so mylib.o -ldl -L../lib

all: server $(PROGS)

server: server.c
	gcc -Wall -o server server.c

clean:
	rm -f *.o *.so $(PROGS)

