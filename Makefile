all:
	gcc -o server.o server.c -lrt -lpthread -g
	gcc -o client.o client.c -lrt -lpthread

mac:
	gcc -o server.o server.c
	gcc -o client.o client.c

clean:
	rm *.o
