LDFLAGS =

ifneq (Darwin, $(shell uname -s))
	LDFLAGS += -lpthread
endif

all:
	gcc -g webserver.c -o webserver.out $(LDFLAGS)

clean:
	rm -f webserver.out

