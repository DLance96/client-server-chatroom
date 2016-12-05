all:
	gcc -o server.o server.c
	gcc -o client.o client.c

clean:
	rm *.o
