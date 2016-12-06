all:
	gcc -o server.o server.c -lrt
	gcc -o client.o client.c

clean:
	rm *.o
