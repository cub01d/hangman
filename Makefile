CC=gcc
CFLAGS=-pthread

all: hangman_server hangman_client

hangman_server: hangman_server.c
	$(CC) -o hangman_server hangman_server.c $(CFLAGS) 

hangman_client: hangman_client.c
	$(CC) -o hangman_client hangman_client.c

clean: 
	rm -f hangman_server hangman_client
