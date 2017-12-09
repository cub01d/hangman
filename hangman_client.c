#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

void print_usage() {
    puts("Usage: ./hangman_client ip port");
    exit(0);
}

void parse_control(char* fromserver, char* word, char* incorrect_guesses) {
    // parses the first argument and splits up string, writing into 
    // `word` and `incorrect_guesses
    uint8_t wordlen = fromserver[1];
    uint8_t numincorrect = fromserver[2];
    
    int i;
    for(i=0; i<wordlen; i++) {
        word[i] = fromserver[3+i];
    }
    for(i=0; i<numincorrect; i++) {
        incorrect_guesses[i] = fromserver[3+wordlen+i];
    }
    // append null terminator
    word[wordlen] = 0;
    incorrect_guesses[numincorrect] = 0;
}

char* parse_message(char* fromserver, char* msg) {
    uint8_t len = fromserver[0];
    char* string = &fromserver[1];
    strncpy(msg, string, len);
    msg[len] = 0;

    fromserver += (len + 1);
    if (fromserver[0] != 0)
        return fromserver;
    else
        return NULL;
}

int main(int argc, char** argv) {
    // check command line args
    if (argc < 3) 
        print_usage();

    // parse argv for host and port
    char* host;
    int port;
    host = argv[1];
    port = atoi(argv[2]);
    if (port <= 0)
        print_usage();
    
    // create socket
    puts("creating socket");
    int fd;
    fd_set readfds;
    FD_ZERO(&readfds);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket creation failed");
        exit(1);
    }
    FD_SET(fd, &readfds);
    
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(host);

    // connect socket
    puts("connecting socket");
    if (connect(fd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0) {
        perror("Socket connection error");
        exit(1);
    }

    // handle server overloaded message
    char from_server[100];
    select(fd+1, &readfds, NULL, NULL, &tv);
    if (FD_ISSET(fd, &readfds)) {
        if (recv(fd, from_server, 20, 0) <= 0) {
            puts("server overloaded. try again later.");
            return 0;
        }
    }

    printf("Ready to start game? (y/n): ");
    fflush(stdout);

    char choice[2];
    fgets(choice, 3, stdin);
    if (choice[0] != 'y')
        exit(0);

    // send empty message to server signaling game start
    char signal = 0;
    send(fd, &signal, 1, MSG_NOSIGNAL);

    // client side game variables
    char* word = (char*) malloc(10 * sizeof(char));         // longest possible word is length 8
    char* incorrect_guesses = (char*) malloc(7 * sizeof(char)); // at most 6 incorrect

    // starting game!
    while(1) {
        // read game control packet from server    
        int bytes_recvd;
        if ((bytes_recvd = recv(fd, from_server, 100, 0)) == 0) {
            puts("server unexpectedly closed the connection.");
            exit(1);
        }

        // parse server packet
        if (from_server[0] != 0) {
            char* msg = (char*) calloc(40, sizeof(char));
            char* next_msg = from_server;
            while(next_msg) {
                next_msg = parse_message(next_msg, msg);
                puts(msg);
            }
            break;
        } else {
            parse_control(from_server, word, incorrect_guesses);
        }

        puts(word);
        printf("Incorrect guesses: %s\n", incorrect_guesses);
        puts("");           // line for readability
        printf("Letter to guess: ");
        fflush(stdout);
        
        // user input loop
        char input;
        int invalid = 1;

        while(invalid) {
            scanf(" %c", &input);
            if (input < 65 || input > 122 || (input > 90 && input < 97)) {
                puts("Error! Please guess one letter.");
                printf("Letter to guess: ");
                fflush(stdout);
            }
            else if (input > 64 && input < 91) {
                input += 32;
                break;
            } else {
                break;
            }
        }
        
        // send user input to server
        char* tosend = (char*) malloc(2 * sizeof(char));
        tosend[0] = 1;
        tosend[1] = input;
        send(fd, tosend, 2, 0);
    }


    close(fd);
    return 0;
}
