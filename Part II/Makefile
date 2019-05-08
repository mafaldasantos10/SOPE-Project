.PHONY: all clean
all: bank

CFLAGS = -Wall
OBJECTS = utility.o user.o

bank: $(OBJECTS)
	@gcc $(CFLAGS) -pedantic $(OBJECTS) -o bank

utility.o user.o: utility.h user.h

clean:
	@rm -f bank $(OBJECTS)