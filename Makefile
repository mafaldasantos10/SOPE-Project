CFLAGS = -Wall -Wextra #to add: -Werror

forensic: forensic.c
	cc $(CFLAGS) -o forensic forensic.c

clean:
	rm forensic