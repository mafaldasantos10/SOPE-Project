CFLAGS = -Wall -Wextra -Werror

forensic: forensic.c
	cc $(CFLAGS) -o forensic forensic.c

clean:
	rm forensic