#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

#define MAX_CONNECTIONS 3
int num_connections = 0;
pthread_mutex_t mutex;   // ensure atomic update of connection count
pthread_t client_thread[MAX_CONNECTIONS];

void print_usage() {
    puts("Usage: ./hangman_server port");
    exit(0);
}

// returns random word in array
const char* get_random_word() {
    int selection = rand() % 15;

    const char* words[15] = 
    {
        // 3 letter words
        "cat",
        "dog",
        "car",
        // 4 letter words
        "girl",
        "rice",
        "jazz",
        // 5 letter words
        "house",
        "pizza",
        "brown",
        // 6 letter words
        "potato",
        "tomato",
        // 7 letter words
        "arsenal",
        "bottles",
        // 8 letter words
        "keyboard",
        "computer"
    };

    return words[selection];
}

char* pack(const char* string) {
    char* newstring = (char*) malloc(50 * sizeof(char));
    newstring[0] = strlen(string);
    strcpy(newstring+1, string); 
    return newstring;
}


// serves hangman games to clients
void* connection_handler(void* desc) {

    // ------------------------------------------------------------------------
    puts("Accepting new client");
    printf("num_connections: %d\n", num_connections);
    // ------------------------------------------------------------------------

    int client_id = num_connections;
    int fd = *(int*) desc;

    // choose game word
    const char* word = get_random_word();
    uint8_t wordlen = strlen(word); 

    // wait for client signal for game to start
    int length = 8;
    char* startsig = (char*) calloc(length, sizeof(char));
    int bytes_recvd = recv(fd, startsig, length, 0);
    if(bytes_recvd !=0 && startsig[0] == 0) {
        // start game
        printf("hangman game started for client %d\n", num_connections);
    
        // server control message strings
        const char youwin[] = "You Win!";
        const char gameover[] = "Game Over!";
        const char youlose[] = "You Lose :(";
        const char thewordwas[] = "The word was: ";

        // game state variables
        char correct_guesses[9] = "";           // longest word is 8 letters long
        char incorrect_guesses[7] = "";         // only allowed up to 6 incorrect guesses
        char* to_guess;                         // word to guess: starts off as underscores
        to_guess = (char*) malloc((wordlen + 1) * sizeof(char));        
        memset(to_guess, '_', wordlen);
        to_guess[wordlen] = 0;                  // null terminator
        int num_correct = 0;                    // used to index correct array
        int num_incorrect = 0;                  // used to index incorrect array

        // main game loop
        while (1) {
            if(num_correct == wordlen || num_incorrect == 6)
                break;
            printf(" ------- num correct:   %d ----------\n", num_correct);
            printf(" ------- num incorrect: %d ----------\n", num_incorrect);
            printf(" ------- wordlen:       %d ----------\n", wordlen);

            // send game control packet to client to update game state
            char* game_state = (char*) malloc(18 * sizeof(char));
            uint8_t control_flag = 0;
            sprintf(game_state, "%c%c%c%s%s", control_flag, wordlen, num_incorrect, to_guess, incorrect_guesses);

            int game_state_len = 3;
            game_state_len += wordlen;
            game_state_len += num_incorrect;

            if(send(fd, game_state, game_state_len, 0) == -1) {
                perror("send failed");
                break;
            }
           
            // receive message from client
            puts("receiving message from client");
            char* client_msg = (char*) malloc(3 * sizeof(char));    // msg length, data, \0
            if (recv(fd, client_msg, length, 0) == 0)
                break;
            if ((uint8_t) client_msg[0] != 1)
                break;
            char client_data = client_msg[1];

            // parse guess into lowercase
            // 65 - 90: A-Z; 97 - 122: a-z
            // out of range
            puts("parsing client message");
            if (client_data < 65 || client_data > 122 || (client_data > 90 && client_data < 97))
                break;
            // uppercase -> convert to lowercase
            else if (client_data > 64 && client_data < 91)
                client_data += 32;
            // make sure guess is lowercase
            assert(client_data > 96 && client_data < 123);
            // make sure guess has not already been guessed
            assert(!strchr(correct_guesses, (int) client_data));
            assert(!strchr(incorrect_guesses, (int) client_data));

            char* guess = strchr(word, (int) client_data);
            if (guess == NULL) {
                // incorrect guess
                incorrect_guesses[num_incorrect++] = client_data;
            } else {
                // correct guess
                correct_guesses[num_correct++] = client_data;
                // update guessed word with underscores
                while (guess != NULL) {
                    int index = guess - word;
                    to_guess[index] = client_data;
                    guess = strchr(guess+1, (int) client_data);
                }
            }
        }
        // send "the word was"
        char* message = (char*) malloc((strlen(thewordwas) + wordlen) * sizeof(char));
        strcpy(message, thewordwas);
        strcat(message, word);
        char* tosend = pack(message);
        if(send(fd, tosend, strlen(tosend), 0) == -1) {
            perror("send failed");
        }

        // ran out of guesses
        if (num_incorrect == 6) {
            // send you lose
            tosend = pack(youlose);
            if(send(fd, tosend, strlen(tosend), 0) == -1) {
                perror("send failed");
            }
        } else {
            // send you win
            tosend = pack(youwin);
            if(send(fd, tosend, strlen(tosend), 0) == -1) {
                perror("send failed");
            }
        }
        // send game over
        tosend = pack(gameover);
        if(send(fd, tosend, strlen(tosend), 0) == -1) {
            perror("send failed");
        }

    } else {
        // illegal client message
        puts("illegal client message");
    }
    printf("Client %d has disconnected.\n", client_id);
    pthread_mutex_lock(&mutex);
    num_connections--;
    pthread_mutex_unlock(&mutex);
    printf(">> num_connections after disconnect: %d\n", num_connections);
    return 0;
}

int main(int argc, char** argv) {
    // check args
    if(argc < 2)
        print_usage();
    
    // parse argv for port number
    int port;
    port = atoi(argv[1]);
    if(port <= 0)
        print_usage();

    // initialize random seed
    srand(time(NULL));

    // setup socket
    // ------------------------------------------------------------------------
    puts("Setting up server...");
    // ------------------------------------------------------------------------
    // ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    int server_fd;
    // create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket creation failed");
        exit(1);
    }
    // set socket options
    int opt = 1;    // set SO_REUSEADDR and SO_REUSEPORT to 1
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof opt)) {
        perror("setsockopt failed");
        exit(1);
    }
    // bind socket to port
    struct sockaddr_in server; 
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr *) &server, sizeof server) < 0) {
        perror("bind failed");
        exit(1);
    }

    // listen for clients
    // handle more than 3 clients inside accept() call
    if (listen(server_fd, 128) < 0) {
        perror("listen failed");
        exit(1);
    }

    // initialize the mutex
    pthread_mutex_init(&mutex, NULL);

    // server overloaded string
    char overloaded[18]; 
    overloaded[0] = 17;
    strcpy(&overloaded[1], "server-overloaded");

    while(1) {
        // new thread for each hangman client
        // main thread rejects new clients if already more than MAX_CONNECTIONS connected
        int addrlen = sizeof server;
        int clientfd;
        
        // ------------------------------------------------------------------------
        puts("Waiting for client connection");
        // ------------------------------------------------------------------------

        // accept client
        if ((clientfd = accept(server_fd, (struct sockaddr *) &server, (socklen_t*) &addrlen)) < 0) {
            perror("accept failed");
            exit(1);
        }
        if (num_connections >= MAX_CONNECTIONS) {
            // accept client and send "server-overloaded" message
            // ------------------------------------------------------------------------
            puts("Rejecting new client...");
            printf("num_connections: %d\n", num_connections);
            // ------------------------------------------------------------------------
            send(clientfd, overloaded, strlen(overloaded), MSG_NOSIGNAL);
            close(clientfd);
        } else {
            pthread_mutex_lock(&mutex);
            // make new pthread to handle client connection
            pthread_t thread = client_thread[num_connections++];
            // specify detached pthread
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            if(pthread_create(&thread, &attr, &connection_handler, (void*) &clientfd) < 0) {
                perror("could not create thread");
                exit(1);
            }
            pthread_mutex_unlock(&mutex);
        }
    }
    puts("!!!! code should never reach here!!!");
    return 0;
}
