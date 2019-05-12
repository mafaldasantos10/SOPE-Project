 all:server.c user.c
	gcc server.c log.c utility.c -o server -lpthread -Wall -Wextra -pedantic -g
	gcc user.c log.c utility.c -o user -lpthread -Wall -Wextra -pedantic -g

clean:
	rm -f server server.o
	rm -f user user.o