all:
	gcc -g webserver.c -o webserver.out

clean:
	rm -f webserver.out

