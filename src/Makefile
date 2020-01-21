PROGS=client server
CFLAGS+=-Wall

-I../include
-L../lib

all: $(PROGS)

clean:
	rm -f *.o $(PROGS)


all: mylib.so

mylib.o: mylib.c
	gcc -Wall -fPIC -DPIC -c mylib.c

mylib.so: mylib.o
	ld -shared -o mylib.so mylib.o -ldl

clean:
	rm -f *.o *.so

