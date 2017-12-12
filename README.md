# Hangman game

Multithreaded hangman game with support for up to 3 simultaneous connections.
Written for UCSB CS176A Computer Networking Fall 2017 by
- Kenneth Chan
- Vanessa Mejia


## Compiling and running

A makefile is included, so just type `make` in the terminal to make the server and the client.

### Server

Running the server is as easy as specifying the port number:

```sh
$ ./hangman_server 12345
```

### Client

Running the client requires both the IP of the server and the port number:

```sh
$ ./hangman_client 127.0.0.1 12345
```

## Features

 - 15 hardcoded words to guess from with lengths varying from 3 to 8
 - More than 3 simultaneous connections results in a `server-overloaded` message from the server
 - Data sent between server and client are packed in a neat custom format
 - Server cannot be killed when `SIGPIPE` from client abruptly closes the connection
 


