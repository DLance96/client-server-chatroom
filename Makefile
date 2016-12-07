all:
	gcc -o server.o server.c -lrt -lpthread
	gcc -o client.o client.c

mac:
	gcc -o server.o server.c
	gcc -o client.o client.c

clean:
	rm *.o
