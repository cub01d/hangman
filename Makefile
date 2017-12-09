CC=gcc
LDFLAGS=-pthread
CFLAGS=-g

all: hangman_server hangman_client

hangman_server: hangman_server.c
	$(CC) -o hangman_server hangman_server.c $(LDFLAGS) $(CFLAGS)

hangman_client: hangman_client.c
	$(CC) -o hangman_client hangman_client.c $(CFLAGS)

clean: 
	rm -f hangman_server hangman_client
